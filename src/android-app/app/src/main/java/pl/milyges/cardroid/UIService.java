package pl.milyges.cardroid;

import android.app.Service;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.PixelFormat;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.IBinder;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout.LayoutParams;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.support.v4.content.LocalBroadcastManager;

import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;

public class UIService extends Service {
    public static final String ACTION_RADIOINFO_SETVISIBLE = "pl.milyges.cardroid.radioinfo_setvisible";

    private static final int _radioSourceIcons[] = {
            -1,
            R.drawable.source_radio48,
            R.drawable.source_cd48,
            R.drawable.source_multimedia48,
            R.drawable.source_aux48
    };

    private WindowManager _windowManager;

    /* Pasek statusu */
    private LinearLayout _statusbarLayout;
    private RadioTextScroller _radioTextScroller;
    private ImageView _sourceIcon;
    private ImageView _bluetoothIcon;
    private ImageView _internetIcon;
    private TextView _radioIcons;
    private boolean _radioInfoVisible = true;
    private boolean _radioOn = false;

    /* Okno głośności */
    private LinearLayout _volumeWindowLayout;
    private WindowManager.LayoutParams _volumeWindowParams;
    private Timer _volumeWindowHideTimer = new Timer();
    private boolean _volumeWindowVisible = false;
    private ProgressBar _volumeBar;

    /* Okno menu radia */
    private LinearLayout _menuWindowLayout;
    private WindowManager.LayoutParams _menuWindowParams;
    private ListView _menuListView;
    private ArrayAdapter<String> _menuListViewAdapter;
    private ArrayList<String> _menuListItems;

    boolean _menuWindowVisible = false;

    private final BroadcastReceiver _bReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            //_log("onReceive, action = " + action);
            if (BluetoothDevice.ACTION_ACL_CONNECTED.equals(action)) {
                /* Bluetooth podłączony */
            }
            else if (BluetoothDevice.ACTION_ACL_DISCONNECTED.equals(action)) {
                /* Bluetooth rozłączony */
            }
            else if (CarDroidService.ACTION_RADIOACTIVITY_CHANGED.equals(action)) {
                /* Włączenie/wyłączenie activity radia (ukrywa/pokazuje tekst w nagłówki */
            }
            else if (CarDroidService.ACTION_RADIOTEXT_CHANGED.equals(action)) {
                /* Zmiana tekstu w radiu */
                if (intent.getBooleanExtra("radioTextChanged", false)) {
                    _radioTextScroller.setText(intent.getStringExtra("radioText"));
                }

                /* Zmiana źródła */
                if (intent.getBooleanExtra("radioSourceChanged", false)) {
                    _sourceIcon.setImageResource(_radioSourceIcons[intent.getIntExtra("radioSource", 0)]);
                }
            }
            else if (CarDroidService.ACTION_RADIOVOLUME_CHANGED.equals(action)) {
                /* Zmiana głośności */
                _volumeWindowHideTimer.cancel();
                _volumeWindowHideTimer.purge();
                _volumeWindowHideTimer = new Timer();
                if (!_volumeWindowVisible) {
                    _windowManager.addView(_volumeWindowLayout, _volumeWindowParams);
                    _volumeWindowVisible = true;
                }

                _volumeBar.setProgress(intent.getIntExtra("radioVolume", 0));
                _volumeWindowHideTimer.schedule(new TimerTask() {
                    @Override
                    public void run() {
                        _windowManager.removeView(_volumeWindowLayout);
                        _volumeWindowVisible = false;
                    }
                }, 3000);
            }
            else if (CarDroidService.ACTION_RADIOMENU_CHANGED.equals(action)) {
                if (intent.getBooleanExtra("radioMenuVisible", false)) {
                    int items = intent.getIntExtra("radioMenuItems", 0);
                    if (items > 0) { /* Pokazujemy okno i wstępnie inicjujemy wpisy */
                        _menuListItems.clear();
                        for(int i = 0; i < items; i++) {
                            _menuListItems.add("");
                        }

                        _windowManager.addView(_menuWindowLayout, _menuWindowParams);
                    }

                    if (intent.getBooleanExtra("radioMenuItemChanged", false)) {
                        int idx = intent.getIntExtra("radioMenuItemID", 0);
                        _menuListItems.set(idx, intent.getStringExtra("radioMenuItemText"));
                        _menuListViewAdapter.notifyDataSetChanged();
                        if (intent.getBooleanExtra("radioMenuItemSelected", false)) {
                            _menuListView.requestFocusFromTouch();
                            _menuListView.setSelection(idx);
                        }
                    }
                }
                else {
                    _windowManager.removeView(_menuWindowLayout);
                    _menuListItems.clear();
                }
            }
            else if (CarDroidService.ACTION_RADIOSTATUS_CHANGED.equals(action)) {
                _radioOn = intent.getBooleanExtra("radioEnabled", false);
                _setRadioInfoVisible();
            }
            else if (CarDroidService.ACTION_RADIOICONS_CHANGED.equals(action)) {
                String icons = "";
                if (intent.getBooleanExtra("radioIconAFRDS", false)) {
                    if (intent.getBooleanExtra("radioIconAFRDSArrow", false)) {
                        icons = icons + "«AF-RDS» ";
                    }
                    else {
                        icons = icons + " AF-RDS  ";
                    }
                }

                if (intent.getBooleanExtra("radioIconNews", false)) {
                    if (intent.getBooleanExtra("radioIconNewsArrow", false)) {
                        icons = icons + "«NEWS» ";
                    }
                    else {
                        icons = icons + " NEWS  ";
                    }
                }

                if (intent.getBooleanExtra("radioIconTraffic", false)) {
                    if (intent.getBooleanExtra("radioIconTrafficArrow", false)) {
                        icons = icons + "«TRAFFIC» ";
                    }
                    else {
                        icons = icons + " TRAFFIC  ";
                    }
                }
                _radioIcons.setText(icons);
            }
            else if (ACTION_RADIOINFO_SETVISIBLE.equals(action)) {
                _radioInfoVisible = intent.getBooleanExtra("radioInfoVisible", false);
                _setRadioInfoVisible();
            }
        }
    };

    private final BroadcastReceiver _bRemoteReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (ConnectivityManager.CONNECTIVITY_ACTION.equals(action)) {
                _updateInternetIcon();
            }
            else if (BluetoothAdapter.ACTION_CONNECTION_STATE_CHANGED.equals(action)) {
                int state = intent.getIntExtra(BluetoothAdapter.EXTRA_CONNECTION_STATE, BluetoothAdapter.STATE_DISCONNECTED);
                if (state == BluetoothAdapter.STATE_CONNECTED) {
                    _bluetoothIcon.setImageResource(R.drawable.bluetooth_active);
                }
                else {
                    _bluetoothIcon.setImageResource(R.drawable.bluetooth_inactive);
                }

            }
        }
    };

    private void _updateInternetIcon() {
        ConnectivityManager connectivityManager = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo netInfo = connectivityManager.getActiveNetworkInfo();
        if ((netInfo != null) && (netInfo.isConnected())) {
            _internetIcon.setImageResource(R.drawable.internet_connected);
        }
        else {
            _internetIcon.setImageResource(R.drawable.internet_disconnected);
        }
    }

    private void _setRadioInfoVisible() {
        if ((_radioOn) && (_radioInfoVisible)) {
            _sourceIcon.setVisibility(ImageView.VISIBLE);
            _radioTextScroller.setVisibility(ImageView.VISIBLE);
            _radioIcons.setVisibility(ImageView.VISIBLE);
        }
        else {
            _sourceIcon.setVisibility(ImageView.INVISIBLE);
            _radioTextScroller.setVisibility(ImageView.INVISIBLE);
            _radioIcons.setVisibility(ImageView.INVISIBLE);
        }
    }
    private void _log(String s) {
        Log.d("UIService", s);
    }

    private void _requestRefresh() {
        Intent i = new Intent(CarDroidService.ACTION_REQUEST_REFRESH);
        LocalBroadcastManager.getInstance(UIService.this).sendBroadcast(i);
    }

    private void _initStatusbar() {
        WindowManager.LayoutParams p;

        _statusbarLayout = (LinearLayout) LayoutInflater.from(this).inflate(R.layout.statusbar_layout, null);
        _statusbarLayout.setLayoutParams(new LayoutParams(30, LayoutParams.MATCH_PARENT));
        _windowManager = (WindowManager)getSystemService(WINDOW_SERVICE);

        p = new WindowManager.LayoutParams(
                800, 56, 0, 0,
                //WindowManager.LayoutParams.TYPE_SYSTEM_OVERLAY,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL | WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN |
                WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.RGB_888);
        p.gravity = Gravity.TOP;

        _windowManager.addView(_statusbarLayout, p);

        _log("statusbar created");

        _radioTextScroller = new RadioTextScroller((TextView)_statusbarLayout.findViewById(R.id.statusBarRadioText), 24, 500);
        _sourceIcon = (ImageView)_statusbarLayout.findViewById(R.id.statusBarSourceIcon);
        _radioIcons = (TextView)_statusbarLayout.findViewById(R.id.statusBarRadioIcons);
        _bluetoothIcon = (ImageView)_statusbarLayout.findViewById(R.id.statusBarBTIcon);
        _internetIcon = (ImageView)_statusbarLayout.findViewById(R.id.statusBarInternetIcon);

        _volumeWindowLayout = (LinearLayout) LayoutInflater.from(this).inflate(R.layout.volumewindow_layout, null);
        _volumeWindowParams = new WindowManager.LayoutParams(
                600, 64, 0, 180,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL | WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN |
                        WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.RGB_888);
        _volumeWindowParams.gravity = Gravity.CENTER_HORIZONTAL;
        _volumeWindowParams.windowAnimations = android.R.style.Animation_Translucent;

        _volumeBar = (ProgressBar)_volumeWindowLayout.findViewById(R.id.volumeProgressBar);

        _menuWindowLayout = (LinearLayout)LayoutInflater.from(this).inflate(R.layout.menuwindow_layout, null);
        _menuWindowParams = new WindowManager.LayoutParams(
                450, 275, 0, 60,
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL | WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN |
                        WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.RGB_888);
        _menuWindowParams.gravity = Gravity.CENTER_HORIZONTAL;
        _menuWindowParams.windowAnimations = android.R.style.Animation_Translucent;

        _menuListView = (ListView)_menuWindowLayout.findViewById(R.id.radioMenuListView);
        _menuListItems = new ArrayList<String>();
        _menuListViewAdapter = new ArrayAdapter<String>(this, R.layout.menuwindowitem_layout, _menuListItems);
        _menuListView.setAdapter(_menuListViewAdapter);

        _requestRefresh();
        _updateInternetIcon();
    }

    public UIService() {
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        _log("onCreate()");
        super.onCreate();

        IntentFilter filter;
        filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_ACL_CONNECTED);
        filter.addAction(BluetoothDevice.ACTION_ACL_DISCONNECTED);
        filter.addAction(CarDroidService.ACTION_RADIOACTIVITY_CHANGED);
        filter.addAction(CarDroidService.ACTION_RADIOTEXT_CHANGED);
        filter.addAction(CarDroidService.ACTION_RADIOICONS_CHANGED);
        filter.addAction(CarDroidService.ACTION_RADIOVOLUME_CHANGED);
        filter.addAction(CarDroidService.ACTION_RADIOMENU_CHANGED);
        filter.addAction(CarDroidService.ACTION_RADIOSTATUS_CHANGED);
        filter.addAction(ACTION_RADIOINFO_SETVISIBLE);

        LocalBroadcastManager.getInstance(this).registerReceiver(_bReceiver, filter);

        filter = new IntentFilter();
        filter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
        filter.addAction(BluetoothAdapter.ACTION_CONNECTION_STATE_CHANGED);
        registerReceiver(_bRemoteReceiver, filter);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        _log("onStart()");
        _initStatusbar();
        return super.onStartCommand(intent, flags, startId);
    }
}
