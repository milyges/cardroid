package pl.milyges.cardroid;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.Settings;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.View;
import android.widget.Toast;

public class MainMenuActivity extends Activity {
    private static final int REQUEST_CODE_CHECKOVERLAYPERM = 1000;
    private static final int REQUEST_CODE_FINELOCATION = 1001;

    private final static String NAVIGATION_PACKAGE = "com.navigation.offlinemaps.gps";

    private PowerManager.WakeLock _wakeLock;

    private void _log(String s) {
        Log.d("MainMenuActivity", s);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main_menu);

        /* Blokujemy usypianie ekranu */
        PowerManager pm = (PowerManager)getSystemService(POWER_SERVICE);
        _wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "CarDroid:wakelock");
        _wakeLock.acquire();

        _checkOverlayPermissions();
    }

    @Override
    protected void onDestroy() {
        _wakeLock.release();
        super.onDestroy();
    }

    @Override
    protected void  onActivityResult(int requestCode, int resultCode,  Intent data) {
        switch (requestCode) {
            case REQUEST_CODE_CHECKOVERLAYPERM:
                _checkGPSPermissions();
                break;
            case REQUEST_CODE_FINELOCATION:
                _startSerivces();
                break;
            default:
                super.onActivityResult(requestCode, resultCode, data);
        }

    }

    private void _checkOverlayPermissions() {
        if (!Settings.canDrawOverlays(this)) {
            Intent i = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION, Uri.parse("package:" + getPackageName()));
            startActivityForResult(i, REQUEST_CODE_CHECKOVERLAYPERM);
        }
        else {
            _checkGPSPermissions();
        }
    }

    private void _checkGPSPermissions() {
        if (ContextCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED) {
            _startSerivces();
        }
        else {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.ACCESS_FINE_LOCATION}, REQUEST_CODE_FINELOCATION);
        }
    }

    private void _startSerivces() {
        _log("Starting UI Service");
        if (Settings.canDrawOverlays(this)) {
            startService(new Intent(this, CarDroidService.class));
            startService(new Intent(this, UIService.class));
        }
        else {
            /* Błąd */
        }
    }


    @Override
    public void onBackPressed() {

    }

    public void openNavigation(View v) {
        Intent naviIntent = getPackageManager().getLaunchIntentForPackage(NAVIGATION_PACKAGE);
        if (naviIntent != null) {
            startActivity(naviIntent);
        }
        else {
            Toast.makeText(this, "Aplikacja nawigacji nie znaleziona", Toast.LENGTH_LONG).show();
        }
    }

    public void openMediaPlayer(View v) {
        Intent naviIntent = getPackageManager().getLaunchIntentForPackage(CarDroidService.MEDIAPLAYER_PACKAGE);
        if (naviIntent != null) {
            startActivity(naviIntent);
        }
        else {
            Toast.makeText(this, "Aplikacja odtwarzacza nie znaleziona", Toast.LENGTH_LONG).show();
        }
    }

    public void openSettings(View v) {
        /* TODO: Własne ustawienia */
        startActivity(new Intent(Settings.ACTION_SETTINGS));
    }

    public void openRadioActivity(View v) {
        Intent i = new Intent(getApplicationContext(), RadioActivity.class);
        i.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        startActivity(i);
    }

    public void openDashboardActivity(View v) {
        Intent i = new Intent(getApplicationContext(), DashboardActivity.class);
        i.setFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);
        startActivity(i);
    }
}
