package pl.milyges.cardroid;

import android.app.Activity;
import android.app.UiAutomation;
import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.ImageView;
import android.widget.TextView;

public class RadioActivity extends Activity {
    private static final int _radioSourceIcons[] = {
            -1,
            R.drawable.source_radio128,
            R.drawable.source_cd128,
            R.drawable.source_usb128,
            R.drawable.source_bt128
    };

    private RadioTextScroller _radioTextScroller;
    private ImageView _sourceIcon;
    private TextView _radioIcons;

    private final BroadcastReceiver _bReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (CarDroidService.ACTION_RADIOTEXT_CHANGED.equals(action)) {
                /* Zmiana tekstu w radiu */
                if (intent.getBooleanExtra("radioTextChanged", false)) {
                    _radioTextScroller.setText(intent.getStringExtra("radioText"));
                }

                /* Zmiana źródła */
                if (intent.getBooleanExtra("radioSourceChanged", false)) {
                    _sourceIcon.setImageResource(_radioSourceIcons[intent.getIntExtra("radioSource", 0)]);
                }
            }
            else if (CarDroidService.ACTION_RADIOSTATUS_CHANGED.equals(action)) {
                if (intent.getBooleanExtra("radioEnabled", false)) {
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
        }
    };

    private void _requestRefresh() {
        Intent i = new Intent(CarDroidService.ACTION_REQUEST_REFRESH);
        LocalBroadcastManager.getInstance(RadioActivity.this).sendBroadcast(i);
    }

    private void _setStatusbarInfoVisible(boolean visible) {
        Intent i = new Intent(UIService.ACTION_RADIOINFO_SETVISIBLE);
        i.putExtra("radioInfoVisible", visible);
        LocalBroadcastManager.getInstance(RadioActivity.this).sendBroadcast(i);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_radio);

        _radioTextScroller = new RadioTextScroller((TextView)findViewById(R.id.radioActivityRadioText), 20, 500);
        _radioIcons = (TextView)findViewById(R.id.radioActivityRadioIcons);
        _sourceIcon = (ImageView)findViewById(R.id.radioActivityRadioSource);

        IntentFilter filter = new IntentFilter();
        filter.addAction(CarDroidService.ACTION_RADIOTEXT_CHANGED);
        filter.addAction(CarDroidService.ACTION_RADIOSTATUS_CHANGED);
        filter.addAction(CarDroidService.ACTION_RADIOICONS_CHANGED);

        LocalBroadcastManager.getInstance(this).registerReceiver(_bReceiver, filter);

        _requestRefresh();
    }

    @Override
    protected void onStart() {
        super.onStart();
        _setStatusbarInfoVisible(false);
        _requestRefresh();
    }

    @Override
    protected void onResume() {
        super.onResume();
        _setStatusbarInfoVisible(false);
        _requestRefresh();
    }

    @Override
    protected void onPause() {
        super.onPause();
        _setStatusbarInfoVisible(true);
        _requestRefresh();
    }

    @Override
    protected void onStop() {
        super.onStop();
        _setStatusbarInfoVisible(true);
        _requestRefresh();
    }
}
