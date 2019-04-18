package pl.milyges.cardroid;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.text.TextPaint;
import android.util.AttributeSet;
import android.view.View;

import java.text.DecimalFormat;

/**
 * TODO: document your custom view class.
 */
public class GaugeView extends View {
    private class GaugeRange {
        public float start;
        public float stop;
    };

    private Paint _framePainter;
    private Paint _bgPainter;
    private Paint _scaleBgPainter;
    private Paint _scalePainter;
    private Paint _valuePainter;
    private TextPaint _namePainter;
    private TextPaint _unitNamePainter;
    private TextPaint _scaleTextPainter;
    private TextPaint _valueTextPainter;

    private RectF _boundingRect;
    private RectF _scaleRect;
    private RectF _valueRect;

    private boolean _sizeChanged = true;
    private float _maxValue = 10;
    private float _minValue = 0;
    private float _scaleTick = 1;
    private float _value = 0;
    private DecimalFormat _valueFormat = new DecimalFormat();

    private boolean _drawFromZero = false;
    private String _name = "";
    private String _unitName = "";

    private void _init(AttributeSet attrs, int defStyle) {
        final TypedArray a = getContext().obtainStyledAttributes(attrs, R.styleable.GaugeView, defStyle, 0);
        _framePainter = new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);
        _framePainter.setColor(Color.parseColor("#808080"));
        _framePainter.setStrokeWidth(5);
        _framePainter.setStyle(Paint.Style.STROKE);

        _bgPainter = new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);
        _bgPainter.setStyle(Paint.Style.FILL);


        _scaleBgPainter = new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);
        _scaleBgPainter.setStyle(Paint.Style.STROKE);
        _scaleBgPainter.setStrokeWidth(10);
        _scaleBgPainter.setColor(Color.BLACK);

        _scalePainter = new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);
        _scalePainter.setColor(Color.WHITE);
        _scalePainter.setStyle(Paint.Style.STROKE);
        _scalePainter.setStrokeWidth(8);

        _valuePainter = new Paint(Paint.ANTI_ALIAS_FLAG|Paint.FILTER_BITMAP_FLAG);
        _valuePainter.setColor(Color.parseColor("#e0e0e0"));
        _valuePainter.setStrokeWidth(9);
        _valuePainter.setStyle(Paint.Style.STROKE);

        _namePainter = new TextPaint();
        _namePainter.setTextSize(22);
        _namePainter.setAntiAlias(true);
        _namePainter.setTextAlign(Paint.Align.LEFT);
        _namePainter.setColor(Color.WHITE);
        _namePainter.setTypeface(Typeface.create("Sans", Typeface.BOLD));

        _unitNamePainter = new TextPaint();
        _unitNamePainter.setTextSize(22);
        _unitNamePainter.setAntiAlias(true);
        _unitNamePainter.setTextAlign(Paint.Align.LEFT);
        _unitNamePainter.setColor(Color.WHITE);
        _unitNamePainter.setTypeface(Typeface.create("Sans", Typeface.BOLD));

        _valueTextPainter = new TextPaint();
        _valueTextPainter.setTextSize(38);
        _valueTextPainter.setAntiAlias(true);
        _valueTextPainter.setTextAlign(Paint.Align.LEFT);
        _valueTextPainter.setColor(Color.WHITE);
        _valueTextPainter.setTypeface(Typeface.create("Sans", Typeface.BOLD));

        _scaleTextPainter = new TextPaint();
        _scaleTextPainter.setTextSize(18);
        _scaleTextPainter.setAntiAlias(true);
        _scaleTextPainter.setTextAlign(Paint.Align.LEFT);
        _scaleTextPainter.setColor(Color.WHITE);
        _scaleTextPainter.setTypeface(Typeface.create("Sans", Typeface.BOLD));

        _valueFormat.setMaximumFractionDigits(0);
        _valueFormat.setMinimumFractionDigits(0);


        _drawFromZero = a.getBoolean(R.styleable.GaugeView_drawFromZero, false);
        _minValue = a.getFloat(R.styleable.GaugeView_minValue, 0);
        _maxValue = a.getFloat(R.styleable.GaugeView_maxValue, 10);
        _value = a.getFloat(R.styleable.GaugeView_value, 0);
        _scaleTick = a.getInteger(R.styleable.GaugeView_scaleTick, 1);
        if (a.hasValue(R.styleable.GaugeView_name)) {
            _name = a.getString(R.styleable.GaugeView_name);
        }

        if (a.hasValue(R.styleable.GaugeView_unitName)) {
            _unitName = a.getString(R.styleable.GaugeView_unitName);
        }

        setValuePrecision(a.getInteger(R.styleable.GaugeView_valuePrecision, 0));
    }

    private void _drawBackground(Canvas c) {
        c.drawRoundRect(_boundingRect, 10, 10, _bgPainter);
        c.drawRoundRect(_boundingRect, 10, 10, _framePainter);
    }

    private void _drawScale(Canvas c) {
        int ticks;
        float anglesPerValue = 180 / (_maxValue - _minValue);
        c.drawArc(_scaleRect, 180, 180, false, _scaleBgPainter);

        ticks = Math.round((_maxValue - _minValue) / _scaleTick);
        for(int i = 0; i <= ticks; i++) {
            c.drawArc(_scaleRect, Math.round(360 - (i * _scaleTick * anglesPerValue)), 2, false, _scalePainter);
        }

        if (_drawFromZero) {
            c.drawArc(_valueRect, 180 + Math.round(-_minValue * anglesPerValue), Math.round(_value * anglesPerValue), false, _valuePainter);
        }
        else {
            c.drawArc(_valueRect, 180, Math.round((_value - _minValue) * anglesPerValue), false, _valuePainter);
        }
    }

    private void _drawLabels(Canvas c) {
        Rect tb = new Rect();
        String tmpStr;

        /* nazwa wskaznika */
        _namePainter.getTextBounds(_name, 0, _name.length(), tb);
        c.drawText(_name, _boundingRect.centerX() - tb.centerX(), _boundingRect.height() - 8, _namePainter);

        /* Jednostka */
        _unitNamePainter.getTextBounds(_unitName, 0, _unitName.length(), tb);
        c.drawText(_unitName, _boundingRect.centerX() - tb.centerX(), 50 + tb.height(), _unitNamePainter);

        /* Aktualna wartosc */
        tmpStr = _valueFormat.format(_value).replace(",", ".");
        _valueTextPainter.getTextBounds(tmpStr, 0, tmpStr.length(), tb);
        c.drawText(tmpStr, _boundingRect.centerX() - tb.centerX(), _boundingRect.centerY() + tb.height(), _valueTextPainter);

        /* Wartosc maks i min na skali */
        tmpStr = _valueFormat.format(_minValue).replace(",", ".");
        _scaleTextPainter.getTextBounds(tmpStr, 0, tmpStr.length(), tb);
        c.drawText(tmpStr, 25 - tb.centerX(), _boundingRect.height() - 24, _scaleTextPainter);

        tmpStr = _valueFormat.format(_maxValue).replace(",", ".");
        _scaleTextPainter.getTextBounds(tmpStr, 0, tmpStr.length(), tb);
        c.drawText(tmpStr,  _boundingRect.width() - 15 - tb.centerX(), _boundingRect.height() - 24, _scaleTextPainter);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (_sizeChanged) {
            _sizeChanged = false;
            _boundingRect = new RectF(canvas.getClipBounds());
            _boundingRect.top += 5;
            _boundingRect.left += 5;
            _boundingRect.right -= 5;
            _boundingRect.bottom -= 5;

            int colors[] = new int[3];
            float positions[] = new float[3];

            colors[0] = Color.parseColor("#202020");
            colors[1] = Color.parseColor("#333333");
            colors[2] = Color.BLACK;
            positions[0] = 0;
            positions[1] = 0.3f;
            positions[2] = 1;
            _bgPainter.setShader(new LinearGradient(
                    0, 0, 0, _boundingRect.height(), colors, positions, Shader.TileMode.MIRROR
            ));

            _scaleRect = new RectF(_boundingRect.left + 20, _boundingRect.top + 20, _boundingRect.right - 20, _boundingRect.right - 20);
            _valueRect = new RectF(_boundingRect.left + 32, _boundingRect.top + 32, _boundingRect.right - 32, _boundingRect.right - 32);
        }

        _drawBackground(canvas);
        _drawScale(canvas);
        _drawLabels(canvas);
    }


    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        _sizeChanged = true;
    }

    public GaugeView(Context context) {
        super(context);
        _init(null, 0);
    }

    public GaugeView(Context context, AttributeSet attrs) {
        super(context, attrs);
        _init(attrs, 0);
    }

    public GaugeView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        _init(attrs, defStyle);
    }


    public float getMaxValue() {
        return _maxValue;
    }

    public void setMaxValue(float _maxValue) {
        this._maxValue = _maxValue;
        invalidate();
    }

    public float getMinValue() {
        return _minValue;
    }

    public void setMinValue(float _minValue) {
        this._minValue = _minValue;
        invalidate();
    }

    public float getScaleTick() {
        return _scaleTick;
    }

    public void setScaleTick(float _scaleTick) {
        this._scaleTick = _scaleTick;
        invalidate();
    }

    public float getValue() {
        return _value;
    }

    public void setValue(float _value) {
        this._value = _value;
        invalidate();
    }

    public boolean isDrawFromZero() {
        return _drawFromZero;
    }

    public void setDrawFromZero(boolean _drawFromZero) {
        this._drawFromZero = _drawFromZero;
        invalidate();
    }

    public String getName() {
        return _name;
    }

    public void setName(String _name) {
        this._name = _name;
        invalidate();
    }

    public String getUnitName() {
        return _unitName;
    }

    public void setUnitName(String _unitName) {
        this._unitName = _unitName;
        invalidate();
    }

    public void setValuePrecision(int precision) {
        _valueFormat.setMaximumFractionDigits(precision);
        _valueFormat.setMinimumFractionDigits(precision);
        invalidate();
    }
}
