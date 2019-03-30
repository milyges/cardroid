package pl.milyges.cardroid;

import android.widget.TextView;
import android.os.Handler;

/* Klasa która będzie "przewijać" tekst w TextView */
class RadioTextScroller {
    private TextView _textView; /* Wskaźnik na TextView */
    private Runnable _scrollTask;
    private Handler _scrollHandler;
    private String _text = ""; /* Tekst */
    private int _textPos; /* Aktualna pozycja wyświetlanego początku */
    private int _maxTextSize; /* Maksymalna ilość znaków wyświetlana na raz */
    private int _delay;

    public RadioTextScroller(TextView tv, int maxTextSize, int delay) {
        _scrollHandler = new Handler();
        _textView = tv;
        _maxTextSize = maxTextSize;
        _delay = 200;

        _scrollTask = new Runnable() {
            @Override
            public void run() {
                if (_text.length() < _maxTextSize) {
                    _textView.setText(_text);
                    _textPos = 0;
                }
                else {
                    if (_text.length() - _textPos < _maxTextSize / 2) {
                        _textPos = 0;
                        _textView.setText(_text.substring(_textPos, _maxTextSize));
                    }
                    else {
                        _textView.setText(_text.substring(_textPos, (_textPos + _maxTextSize >= _text.length()) ? _text.length() : _textPos + _maxTextSize));
                        _textPos++;
                        _scrollHandler.postDelayed(_scrollTask, _delay);
                    }
                }
            }
        };
    }

    public void setText(String s) {
        if (!_text.equals(s)) {
            _text = s;
            _textPos = 0;
            _scrollHandler.post(_scrollTask);
        }
    }

    public void setMaxTextSize(int maxTextSize) {
        _maxTextSize = maxTextSize;
    }

    public void setDelay(int delay) {
        _delay = delay;
    }

    public void setVisibility(int visibility) { _textView.setVisibility(visibility); }
}
