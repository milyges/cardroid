package pl.milyges.cardroid;

import android.app.Service;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.IBinder;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;

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

    private CarDroidSerial _serial;
    private String _radioText;
    private int _radioSource;
    private boolean _radioSourcePaused;

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

            while (true) {
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

    private void _shutdown() {
        try {
            Runtime.getRuntime().exec(new String[]{ "su", "-c", "svc power shutdown"});
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
            _radioTextChanged(_radioText, source);
        }
        else {
            _radioTextChanged(text, source);
        }
    }

    @Override
    public void onCreate() {
        _log("onCreate");
        super.onCreate();
        _serial = new CarDroidSerial();
        _radioText = "";
    }

    @Override
    public IBinder onBind(Intent intent) {
        // TODO: Return the communication channel to the service.
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        _log("onStartCommand");

        _serial.start();
        _serial.sendCommand(CMD_DISPLAY_REFRESH);
        _serial.sendCommand(CMD_POWER_STATUS);

        IntentFilter filter = new IntentFilter();
        filter.addAction(ACTION_REQUEST_REFRESH);
        LocalBroadcastManager.getInstance(this).registerReceiver(_bReceiver, filter);

        return super.onStartCommand(intent, flags, startId);
    }
}
