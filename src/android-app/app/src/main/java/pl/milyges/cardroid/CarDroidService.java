package pl.milyges.cardroid;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothA2dp;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.SystemClock;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;
import android.view.KeyEvent;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.text.DateFormat;
import java.text.FieldPosition;
import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.List;
import java.util.TimeZone;

public class CarDroidService extends Service {
    /* Typy źródeł dźwięku */
    public static final int SOURCE_UNKNOWN = 0;
    public static final int SOURCE_FMRADIO = 1;
    public static final int SOURCE_CD = 2;
    public static final int SOURCE_MULTIMEDIA = 3;
    public static final int SOURCE_AUX = 4;

    /* Ikony radia */
    public static final int RADIO_ICON_NO_NEWS = 1 << 0;
    public static final int RADIO_ICON_NEWS_ARROW = 1 << 1;
    public static final int RADIO_ICON_NO_TRAFFIC = 1 << 2;
    public static final int RADIO_ICON_TRAFFIC_ARROW = 1 << 3;
    public static final int RADIO_ICON_NO_AFRDS = 1 << 4;
    public static final int RADIO_ICON_AFRDS_ARROW = 1 << 5;
    public static final int RADIO_ICON_NO_MODE = 1 << 6;

    /* Typy komunikatów */
    public static final String ACTION_RADIOTEXT_CHANGED = "pl.milyges.cardroid.radiotextchanged";
    public static final String ACTION_RADIOICONS_CHANGED = "pl.milyges.cardroid.radioiconschanged";
    public static final String ACTION_RADIOACTIVITY_CHANGED = "pl.milyges.cardroid.radioactivitychanged";
    public static final String ACTION_RADIOSTATUS_CHANGED = "pl.milyges.cardroid.radiostatuschanged";
    public static final String ACTION_RADIOVOLUME_CHANGED = "pl.milyges.cardroid.radiovolumechanged";
    public static final String ACTION_RADIOMENU_CHANGED = "pl.milyges.cardroid.radiomenuchanged";
    public static final String ACTION_REQUEST_REFRESH = "pl.milyges.cardroid.requestrefresh";

    /* Polecenia do mikrokontrolera */
    public static final String CMD_DISPLAY_REFRESH = "+DR";
    public static final String CMD_POWER_STATUS = "+PS";
    public static final String CMD_POWER_OFF = "+POFF";
    public static final String CMD_BACKLIGHT_OFF = "+BOFF";
    public static final String CMD_BACKLIGHT_ON = "+BON";

    /* Aplikacja do otwarzania muzyki */
    public final static String MEDIAPLAYER_PACKAGE = "com.musicplayer.playermusic";

    /* Akcje łapane od playera */
    public static final String ACTION_MEDIAPLAYER_METACHANGED = MEDIAPLAYER_PACKAGE + ".metachanged";
    public static final String ACTION_MEDIAPLAYER_PLAYSTATECHANGED = MEDIAPLAYER_PACKAGE + ".playstatechanged";
    public static final String ACTION_MEDIAPLAYER_REFRESH = MEDIAPLAYER_PACKAGE + ".refresh";

    private CarDroidSerial _serial;
    private String _radioText;
    private int _radioSource;
    private boolean _radioSourcePaused;
    private AudioManager _audioManager = null;
    private String _mediaplayerRadioText = "";

    private final BroadcastReceiver _bReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            if (ACTION_REQUEST_REFRESH.equals(action)) {
                _radioText = "";
                _radioSource = SOURCE_UNKNOWN;

                _serial.sendCommand(CMD_DISPLAY_REFRESH);
            }
        }
    };

    private final BroadcastReceiver _bRemoteReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();

            //_log("_bRemoteReceiver: action=" + action);

            if (ACTION_MEDIAPLAYER_METACHANGED.equals(action)) {
                String track = intent.getStringExtra("track");
                String artist = intent.getStringExtra("artist");
                if (artist == null) {
                    artist = "<unknown>";
                }

                if (!artist.equals("<unknown>")) {
                    _mediaplayerRadioText = artist + " - " + track;
                }
                else {
                    _mediaplayerRadioText = track;
                }

                if(_radioSource == SOURCE_MULTIMEDIA) {
                    _radioTextChanged(_mediaplayerRadioText, _radioSource);
                }
            }
        }
    };

    private final LocationListener _locationListener = new LocationListener() {
        private float _minAccuracy = 9999;

        @Override
        public void onLocationChanged(Location location) {
            if (location.hasAccuracy()) {
                if (location.getAccuracy() < _minAccuracy) { /* Jak mamy dokładność poniżej 10m to mamy fixa */
                    _minAccuracy = location.getAccuracy();
                    if (_minAccuracy < 10) { /* Poniżej 10m to na pewno mamy fixa ;) */
                        _minAccuracy = 0;
                    }
                    _setDateTime(location.getTime());
                }
            }
        }

        @Override
        public void onStatusChanged(String provider, int status, Bundle extras) {

        }

        @Override
        public void onProviderEnabled(String provider) {

        }

        @Override
        public void onProviderDisabled(String provider) {

        }
    };

    class CarDroidSerial extends Thread {
        private RandomAccessFile _ttyFile = null;

        public CarDroidSerial() {
            try {
                _ttyFile = new RandomAccessFile("/dev/ttyS1", "rw");
            } catch (FileNotFoundException e) {
                e.printStackTrace();
            }
        }

        public void sendCommand(String cmd) {
            _log("send command " + cmd);
            try {
                _ttyFile.write((cmd + "\n").getBytes(StandardCharsets.US_ASCII));
            } catch (IOException e) {
                e.printStackTrace();
            }
        }


        public void run() {
            super.run();
            _log("Serial init.");
            try {
                Runtime.getRuntime().exec("stty -F /dev/ttyS1 19200 -echo");
            } catch (IOException e) {
                e.printStackTrace();
            }
            _log("Serial ready.");

            while (!isInterrupted()) {
                try {
                    String line = _ttyFile.readLine();

                    if (!line.isEmpty()) {
                        if (line.startsWith("+RADIOTEXT:")) {
                            _radioTextRecv(line.substring("+RADIOTEXT:".length()));
                        }
                        else if (line.startsWith("+MENU:")) {
                            _radioMenuRecv(line.substring("+MENU:".length()));
                        }
                        else if (line.startsWith("+RADIO:")) {
                            _radioStatusRecv(line.substring("+RADIO:".length()));
                        }
                        else if (line.startsWith("+RADIOICONS:")) {
                            _radioIconsRecv(line.substring("+RADIOICONS:".length()));
                        }
                        else if (line.startsWith("+CDC:")) {
                            _cdcCmdRecv(line.substring("+CDC:".length()));
                        }
                        else if (line.startsWith("+SHUTDOWN")) {
                            _log("Shutdown system.");
                            _shutdown();
                        }
                        else {
                            _log("Unknown data from mainboard:" + line);
                        }
                    }
                }
                catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private void _log(String s) {
        Log.d("CarDroidSerivce", s);
    }

    private void _sendMediaKey(int key) {
        if (_audioManager == null) {
            _audioManager = (AudioManager)getSystemService(Context.AUDIO_SERVICE);
        }

        long eventTime = SystemClock.uptimeMillis();
        _audioManager.dispatchMediaKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN, key, 0));
        _audioManager.dispatchMediaKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_UP, key, 0));
    }

    private void _shutdown() {
        _bluetoothSetState(false); /* Wyłączamy BT przed zamknięciem systemu */
        try {
            Runtime.getRuntime().exec(new String[]{ "su", "-c", "svc power shutdown"});
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void _setDateTime(long timestamp) {
        SimpleDateFormat sdf = new SimpleDateFormat("MMddHHmmyyyy.ss");
        sdf.setTimeZone(TimeZone.getTimeZone("UTC"));
        Date d = new Date(timestamp);
        try {
            Runtime.getRuntime().exec(new String[]{ "su", "-c", "date -u " + sdf.format(d)});
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void _radioTextChanged(String newText, int newSource) {
        boolean textChanged = false, sourceChanged = false;
        synchronized (_radioText) {
            textChanged = !_radioText.equals(newText);
            sourceChanged = _radioSource != newSource;

            _radioText = newText;
            _radioSource = newSource;

            Intent i = new Intent(CarDroidService.ACTION_RADIOTEXT_CHANGED);
            i.putExtra("radioTextChanged", textChanged);
            i.putExtra("radioText", _radioText);
            i.putExtra("radioSourceChanged", sourceChanged);
            i.putExtra("radioSource", _radioSource);
            LocalBroadcastManager.getInstance(CarDroidService.this).sendBroadcast(i);
        }
    }

    private void _radioVolumeChanged(int newValue) {
        Intent i = new Intent(CarDroidService.ACTION_RADIOVOLUME_CHANGED);
        i.putExtra("radioVolume", newValue);
        LocalBroadcastManager.getInstance(CarDroidService.this).sendBroadcast(i);
    }

    private void _radioMenuRecv(String text) {
        String args[] = text.split(",");
        Intent i = new Intent(CarDroidService.ACTION_RADIOMENU_CHANGED);
        if (args[0].equals("ON")) {
            i.putExtra("radioMenuVisible", true);
            i.putExtra("radioMenuItems", Integer.valueOf(args[1]));
        }
        else if (args[0].equals("OFF")) {
            i.putExtra("radioMenuVisible", false);
        }
        else if (args[0].equals("ITEM")) {
            i.putExtra("radioMenuVisible", true);
            i.putExtra("radioMenuItemChanged", true);
            i.putExtra("radioMenuItemID", Integer.valueOf(args[1]));
            i.putExtra("radioMenuItemText", args[2]);
            i.putExtra("radioMenuItemSelected", args[3].equals("1"));
        }

        LocalBroadcastManager.getInstance(CarDroidService.this).sendBroadcast(i);
    }

    private void _radioStatusRecv(String text) {
        Intent i = new Intent(CarDroidService.ACTION_RADIOSTATUS_CHANGED);
        i.putExtra("radioEnabled", text.equals("ON"));
        LocalBroadcastManager.getInstance(CarDroidService.this).sendBroadcast(i);
    }

    private void _radioIconsRecv(String text) {
        int icons;

        try {
            icons = Integer.parseInt(text);
        }
        catch (NumberFormatException e) {
            _log("_radioIconsRecv: NumberFormatException: " + text);
            return;
        }

        Intent i = new Intent(CarDroidService.ACTION_RADIOICONS_CHANGED);
        if ((icons & RADIO_ICON_NO_NEWS) == RADIO_ICON_NO_NEWS) {
            i.putExtra("radioIconNews", false);
        }
        else {
            i.putExtra("radioIconNews", true);
        }

        if ((icons & RADIO_ICON_NEWS_ARROW) == RADIO_ICON_NEWS_ARROW) {
            i.putExtra("radioIconNewsArrow", true);
        }
        else {
            i.putExtra("radioIconNewsArrow", false);
        }

        if ((icons & RADIO_ICON_NO_TRAFFIC) == RADIO_ICON_NO_TRAFFIC) {
            i.putExtra("radioIconTraffic", false);
        }
        else {
            i.putExtra("radioIconTraffic", true);
        }

        if ((icons & RADIO_ICON_TRAFFIC_ARROW) == RADIO_ICON_TRAFFIC_ARROW) {
            i.putExtra("radioIconTrafficArrow", true);
        }
        else {
            i.putExtra("radioIconTrafficArrow", false);
        }

        if ((icons & RADIO_ICON_NO_AFRDS) == RADIO_ICON_NO_AFRDS) {
            i.putExtra("radioIconAFRDS", false);
        }
        else {
            i.putExtra("radioIconAFRDS", true);
            /* Jeżeli jest AF-RDS to na pewno jest to radio FM */
            if (_radioSource != SOURCE_FMRADIO) {
                _radioTextChanged(_radioText, SOURCE_FMRADIO);
            }
        }

        if ((icons & RADIO_ICON_AFRDS_ARROW) == RADIO_ICON_AFRDS_ARROW) {
            i.putExtra("radioIconAFRDSArrow", true);
        }
        else {
            i.putExtra("radioIconAFRDSArrow", false);
        }

        if ((icons & RADIO_ICON_NO_MODE) == RADIO_ICON_NO_MODE) {
            i.putExtra("radioIconMode", false);
        }
        else {
            i.putExtra("radioIconMode", true);
        }

        LocalBroadcastManager.getInstance(CarDroidService.this).sendBroadcast(i);
    }
    private void _radioTextRecv(String text) {
        int source = _radioSource;

        if (text.equals("   PAUSE    ")) {
            synchronized (_radioText) {
                if (!_radioSourcePaused) {
                    _radioSourcePaused = true;
                    //emit pausestate changed
                    _radioTextChanged(text, _radioSource);
                }
            }
            return;
        }

        synchronized (_radioText) {
            if (_radioSourcePaused) {
                _radioSourcePaused = false;
                //emit pausestate changed
            }
        }

        if (text.startsWith("VOLUME ")) {
            _radioVolumeChanged(Integer.valueOf(text.substring(7, 9).trim()));
            return;
        }

        /* Detekcja źródła */
        if (text.equals("AUX         ")) {
            source = SOURCE_AUX;
        }
        else if ((text.equals("CD          ")) || (text.equals("  LOAD CD   ")) || (text.equals("  READ CD   ")) ||
                (text.equals("   MP3 CD   ")) || (text.startsWith("ALB ")) || (text.startsWith("LOAD ALB ")) || (text.length() > 12)) {
            source = SOURCE_CD;
        }
        else if ((text.equals("CD CHANGER  ")) || (text.startsWith("CD "))) {
            source = SOURCE_MULTIMEDIA;
        }
        else if (text.equals("RADIO FM    ")) {
            source = SOURCE_FMRADIO;
        }

        if (source == SOURCE_MULTIMEDIA) {
            _radioTextChanged(_mediaplayerRadioText, source);
        }
        else {
            _radioTextChanged(text, source);
        }
    }

    private void _cdcCmdRecv(String text) {
        String args[] = text.split(",");
        if (args[0].equals("PLAY")) {
            _sendMediaKey(KeyEvent.KEYCODE_MEDIA_PLAY);
        }
        else if (args[0].equals("PAUSE")) {
            _sendMediaKey(KeyEvent.KEYCODE_MEDIA_PAUSE);
        }
        else if (args[0].equals("STOP")) {
            _sendMediaKey(KeyEvent.KEYCODE_MEDIA_PAUSE);
        }
        else if (args[0].equals("NEXT")) {
            _sendMediaKey(KeyEvent.KEYCODE_MEDIA_NEXT);
        }
        else if (args[0].equals("PREV")) {
            _sendMediaKey(KeyEvent.KEYCODE_MEDIA_PREVIOUS);
        }
    }

    private void _bluetoothSetState(boolean enabled) {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter.isEnabled() != enabled) {
            if (enabled) {
                adapter.enable();
            }
            else {
                adapter.disable();
            }
        }
    }

    @Override
    public void onCreate() {
        _log("onCreate");
        super.onCreate();
        _serial = new CarDroidSerial();
        _serial.start();
        _radioText = "";
    }

    @Override
    public IBinder onBind(Intent intent) {
        // TODO: Return the communication channel to the service.
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        IntentFilter filter;

        _log("onStartCommand");
        _serial.sendCommand(CMD_DISPLAY_REFRESH);
        _serial.sendCommand(CMD_POWER_STATUS);

        filter = new IntentFilter();
        filter.addAction(ACTION_REQUEST_REFRESH);
        LocalBroadcastManager.getInstance(this).registerReceiver(_bReceiver, filter);

        filter = new IntentFilter();
        /* Komunikaty od mediaplayera */
        filter.addAction(ACTION_MEDIAPLAYER_METACHANGED);
        filter.addAction(ACTION_MEDIAPLAYER_PLAYSTATECHANGED);
        filter.addAction(ACTION_MEDIAPLAYER_REFRESH);

        registerReceiver(_bRemoteReceiver, filter);

        LocationManager locationManager = (LocationManager)getSystemService(LOCATION_SERVICE);
        locationManager.requestLocationUpdates(LocationManager.GPS_PROVIDER, 2000, 0, _locationListener);

        _bluetoothSetState(false);
        
        _bluetoothSetState(true);
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        _serial.interrupt();
        super.onDestroy();
    }
}
