// =========================
// Axis
// =========================
class _Axis {
  constructor(name, min = -10, max = 10, tickLength = 6, drawAt = 0) {
    this._name = name;
    this._autoScale = (min == max); // if min == max, that means AutoScale
    this._min = min;
    this._range = max - min;
    this._tickLength = tickLength; // if < 0, that means full length
    this._drawAt = drawAt; // int means offset [pix] from min (if < 0, from max), or AxisObject means this Axis is drawn at '0' of AxisObject
    this._drawName = false;
  }

  name() { return this._name; }
  min() { return this._min; }
  max() { return this._min + this._range; }
  range() { return this._range; }
  tickLength() { return this._tickLength; }
  drawAt(drawAt=null) { if(drawAt!=null){ this._drawAt = drawAt; } return this._drawAt; }
  drawName(b=null) { if(b!=null){ this._drawName = b;} return this._drawName; }
  autoScale() { return this._autoScale; }

  setMinMax(min=null, max=null) {
    if(min == null && max == null) return;
    else if(min != null && max == null){ this._min = min; }
    else if(min == null && max != null){ this._min = max - this._range; }
    else{ this._min = min; this._range = max - min; }
  }

  createTicks(targetCount = 6) {
    if (this._range <= 0) return [];

    const roughStep = this._range / targetCount;
    const pow = Math.pow(10, Math.floor(Math.log10(roughStep)));
    const steps = [1, 2, 5].map(v => v * pow);
    const step = steps.reduce((prev, curr) =>
      Math.abs(curr - roughStep) < Math.abs(prev - roughStep) ? curr : prev
    );

    const start = Math.ceil(this._min / step) * step;
    const ticks = [];
    for (let v = start; v <= this.max(); v += step) { ticks.push(v); }
    return ticks;
  }
}

// =========================
// DataSeries2D
// =========================
class _DataSeries2D {
  constructor(name, color, xAxis, yAxis, drawPoints = true, drawLines = true) {
    this._name = name;
    this._color = color;
    this._drawPoints = drawPoints;
    this._drawLines = drawLines;
    this._xAxis = xAxis;
    this._yAxis = yAxis;

    this.x = [];
    this.y = [];
  }

  name() { return this._name; }
  color() { return this._color; }
  drawPoints(b=null) { if(b!=null){ this._drawPoints = b; } return this._drawPoints; }
  drawLines(b=null) { if(b!=null){ this._drawLines = b; } return this._drawLines; }
  xAxis() { return this._xAxis; }
  yAxis() { return this._yAxis; }

  addData(x, y) {
    for (const [axis, v] of [[this._xAxis, x], [this._yAxis, y]]) {
      if(axis.autoScale()){
        if(axis.range() == 0){ axis.setMinMax(v-1e-12, v+1e-12); }
        else if(v > axis.max()){ axis.setMinMax(axis.min(), v); }
        else if(v < axis.min()){ axis.setMinMax(v, axis.max()); }
      }
    }
    this.x.push(x);
    this.y.push(y);
  }

  setData(xArray, yArray) {
    this.x = [];
    this.y = [];
    for(let i = 0; i < xArray.length; i++) {
      this.addData(xArray[i], yArray[i]);
    }
  }

  delOldX() {
    while (this.x.length > 1 && this.x[1] < this._xAxis.min()) {
        this.x.shift();
        this.y.shift();
    }
  }
}

// =========================
// Plot2D
// =========================
class Plot2D {
  constructor(canvas, heightRatio=0.8, xRange=[0, 0], yRange=[0, 0], xAxisName="X", yAxisName="Y") {
    this._canvas = canvas;
    this._ctx = canvas.getContext("2d");
    this._dpr = window.devicePixelRatio || 1;

    this._viewHeightRatio = heightRatio;
    this._xAxes = [new _Axis(xAxisName, xRange[0], xRange[1], -1)];
    this._yAxes = [new _Axis(yAxisName, yRange[0], yRange[1], -1)];
    this._xAxes[0].drawAt(this._yAxes[0]);
    this._yAxes[0].drawAt(this._xAxes[0]);

    this._seriesList = [];

    this.resize();
    window.addEventListener("resize", () => this.resize());
  }

  addXAxis(name, xRange=[0,0], tickLength = undefined, drawAt = undefined) {
    const a = new _Axis(name, xRange[0], xRange[1], tickLength, drawAt);
    this._xAxes.push(a);
    return a;
  }
  getXAxis(i=0){ return this._xAxes[i]; }

  addYAxis(name, yRange=[0,0], tickLength = undefined, drawAt = undefined) {
    const a = new _Axis(name, yRange[0], yRange[1], tickLength, drawAt);
    this._yAxes.push(a);
    return a;
  }
  getYAxis(i=0){ return this._yAxes[i]; }
  
  getSeriesNameList() {
    const series_name_list = [];
    for(const s of this._seriesList) series_name_list.push(s.name());
    return series_name_list;
  }
  addSeries(name, color, x = null, y = null, drawPoints = true, drawLine = true, xAxis = this._xAxes[0], yAxis = this._yAxes[0]) {
    const s = new _DataSeries2D(name, color, xAxis, yAxis, drawPoints, drawLine);
    if (!y) y = [];
    if (!x) x = [...Array(y.length).keys()];
    s.setData(x, y);
    this._seriesList.push(s);
    return s;
  }
  getSeries(name) {
    return this._seriesList.find(s => s.name() === name);
  }

  // 高DPI対応リサイズ
  resize() {
    const rect = this._canvas.getBoundingClientRect();

    this._canvas.width = rect.width * this._dpr;
    this._canvas.height = rect.width * this._dpr * this._viewHeightRatio;

    this._ctx.setTransform(this._dpr, 0, 0, this._dpr, 0, 0);

    this.render();
  }

  _toScreen(x, y, xAxis = undefined, yAxis = undefined) {
    let sx = x;
    if (xAxis) {
      const w = this._canvas.width / this._dpr;
      sx = (x - xAxis.min()) / Math.max(xAxis.range(), 1e-12) * w;
    }

    let sy = y;
    if (yAxis) {
      const h = this._canvas.height / this._dpr;
      sy = h - (y - yAxis.min()) / Math.max(yAxis.range(), 1e-12) * h;
    }

    return [sx, sy];
  }

  clear() {
    const w = this._canvas.width / this._dpr;
    const h = this._canvas.height / this._dpr;
    this._ctx.clearRect(0, 0, w, h);
  }

  // TODO: データ数が多くなったら連続描画（polyline）を検討 
  _drawLine(x1, y1, x2, y2, xAxis = undefined, yAxis = undefined, color = "#888", width = 1) {
    const [sx1, sy1] = this._toScreen(x1, y1, xAxis, yAxis);
    const [sx2, sy2] = this._toScreen(x2, y2, xAxis, yAxis);

    this._ctx.beginPath();
    this._ctx.moveTo(sx1, sy1);
    this._ctx.lineTo(sx2, sy2);
    this._ctx.strokeStyle = color;
    this._ctx.lineWidth = width;
    this._ctx.stroke();
  }

  _drawPoint(x, y, xAxis = undefined, yAxis = undefined, r = 4, color = "red") {
    const [sx, sy] = this._toScreen(x, y, xAxis, yAxis);

    this._ctx.beginPath();
    this._ctx.arc(sx, sy, r, 0, Math.PI * 2);
    this._ctx.fillStyle = color;
    this._ctx.fill();
  }

  _drawAxes() {
    const ctx = this._ctx;
    const w = this._canvas.width / this._dpr;
    const h = this._canvas.height / this._dpr;

    // X軸の描画Y座標
    const xAxes_sy = [];
    for (const xAxis of this._xAxes)
    {
      let xaxis_sy;
      if (xAxis.drawAt() instanceof _Axis) {
        xaxis_sy = this._toScreen(0, 0, null, xAxis.drawAt())[1];
      } else if (xAxis.drawAt() < 0) {
        xaxis_sy = h + xAxis.drawAt();
      } else {
        xaxis_sy = xAxis.drawAt();
      }
      xaxis_sy = xaxis_sy < 0 ? 0 : (xaxis_sy > h ? h : xaxis_sy);
      xAxes_sy.push(xaxis_sy);
    }
    // Y軸の描画X座標
    const yAxes_sx = [];
    for (const yAxis of this._yAxes)
    {
      let yaxis_sx;
      if (yAxis.drawAt() instanceof _Axis) {
        yaxis_sx = this._toScreen(0, 0, yAxis.drawAt(), null)[0];
      } else if (yAxis.drawAt() < 0) {
        yaxis_sx = w + yAxis.drawAt();
      } else {
        yaxis_sx = yAxis.drawAt();
      }
      yaxis_sx = yaxis_sx < 0 ? 0 : (yaxis_sx > w ? w : yaxis_sx);
      yAxes_sx.push(yaxis_sx);
    }

    // X軸の目盛り線X座標
    const xAxes_ticks = []
    const xAxes_ticks_sx = []
    for (const xAxis of this._xAxes) {
        const xAxis_ticks = xAxis.createTicks()
        xAxes_ticks.push(xAxis_ticks)
        xAxes_ticks_sx.push(xAxis_ticks.map((t) => this._toScreen(t, 0, xAxis, null)[0]));
    }
    // Y軸の目盛り線Y座標
    const yAxes_ticks = []
    const yAxes_ticks_sy = []
    for (const yAxis of this._yAxes) {
        const yAxis_ticks = yAxis.createTicks()
        yAxes_ticks.push(yAxis_ticks)
        yAxes_ticks_sy.push(yAxis_ticks.map((t) => this._toScreen(0, t, null, yAxis)[1]));
    }

    // 目盛り線
    for (let i = 0; i < xAxes_ticks_sx.length; i++) {
        const sy = xAxes_sy[i];
        const tickLength = this._xAxes[i].tickLength();
        const [y0, y1] = (tickLength < 0 ? [0, h] : [sy - tickLength, sy + tickLength]);
        for (const sx of xAxes_ticks_sx[i]) this._drawLine(sx, y0, sx, y1, null, null, "#ccc", 1);
    }
    for (let i = 0; i < yAxes_ticks_sy.length; i++) {
        const sx = yAxes_sx[i];
        const tickLength = this._yAxes[i].tickLength();
        const [x0, x1] = (tickLength < 0 ? [0, w] : [sx - tickLength, sx + tickLength])
        for (const sy of yAxes_ticks_sy[i]) this._drawLine(x0, sy, x1, sy, null, null, "#ccc", 1);
    }

    // 軸
    for (const sy of xAxes_sy) this._drawLine(0, sy, w, sy, null, null, "#888", 2);
    for (const sx of yAxes_sx) this._drawLine(sx, 0, sx, h, null, null, "#888", 2);

    // ラベル
    ctx.fillStyle = "#444";
    const X_TICK_LABEL_MARGIN = 5;
    for (let i = 0; i < xAxes_sy.length; i++) {
        const xAxis = this._xAxes[i];
        const sy = xAxes_sy[i];
        const label_sy = (sy < h / 2 ? sy + 6 : sy - 6);
        ctx.textBaseline = (sy < h / 2 ? "top" : "bottom");
        
        // 軸ラベル
        let AxisNameXmin = w;
        if ( xAxis.drawName() ) {
            ctx.font = "bold 12px sans-serif";
            ctx.textAlign = "left";
            AxisNameXmin = w - (5 + ctx.measureText(xAxis.name()).width + 5);
            ctx.fillText(xAxis.name(), AxisNameXmin + 5, label_sy);
        }

        // 目盛りラベル
        ctx.font = "12px sans-serif";
        ctx.textAlign = "center";
        for (let j = 0; j < xAxes_ticks[i].length; j++) {
            const t = xAxes_ticks[i][j];
            const sx = xAxes_ticks_sx[i][j];

            // 端すぎたり、Y軸や軸ラベルと被る目盛りラベルは描画しない
            let skip = false;
            if (sx < X_TICK_LABEL_MARGIN) continue;
            if (sx > AxisNameXmin - X_TICK_LABEL_MARGIN) continue;
            for (const yaxis_sx of yAxes_sx) {
              if (sx >= yaxis_sx - X_TICK_LABEL_MARGIN && sx <= yaxis_sx + X_TICK_LABEL_MARGIN) { skip = true; break; }
            }
            if (skip) continue;

            let t_ = Number(t.toFixed(12));
            let label = t_.toString(); if (label.length >= 7) label = t_.toExponential(2);
            ctx.fillText(label, sx, label_sy);
        }
    }
    const Y_TICK_LABEL_MARGIN = 6
    for (let i = 0; i < yAxes_sx.length; i++) {
        const yAxis = this._yAxes[i];
        const sx = yAxes_sx[i];
        const label_sx = (sx > w / 2 ? sx - 6 : sx + 6);
        ctx.textAlign = (sx > w / 2 ? "right" : "left");
        
        // 軸ラベル
        let AxisNameYmax = 0;
        if ( yAxis.drawName() ) {
            ctx.font = "bold 12px sans-serif";
            ctx.textBaseline = "bottom";
            const m = ctx.measureText(yAxis.name());
            AxisNameYmax = 5 + (m.fontBoundingBoxAscent + m.fontBoundingBoxDescent) + 5;
            ctx.fillText(yAxis.name(), label_sx, AxisNameYmax - 5);
        }

        // 目盛りラベル
        ctx.font = "12px sans-serif";
        ctx.textBaseline = "middle";
        for (let j = 0; j < yAxes_ticks[i].length; j++) {
            const t = yAxes_ticks[i][j];
            const sy = yAxes_ticks_sy[i][j];
  
            // 端すぎたり、Y軸や軸ラベルと被る目盛りラベルは描画しない
            let skip = false;
            if (sy < AxisNameYmax + Y_TICK_LABEL_MARGIN) continue;
            if (sy > h - Y_TICK_LABEL_MARGIN) continue;
            for(const xaxis_sy of xAxes_sy) {
                if (sy >= xaxis_sy - Y_TICK_LABEL_MARGIN && sy <= xaxis_sy + Y_TICK_LABEL_MARGIN) { skip = true; break; }
            }
            if (skip) continue;
  
            let t_ = Number(t.toFixed(12));
            let label = t_.toString(); if (label.length >= 7) label = t_.toExponential(2);
            ctx.fillText(label, label_sx, sy);
        }
    }
  }

  _drawLegend() {
    // 凡例描画
    const ctx = this._ctx;
    const w = this._canvas.width / this._dpr;
    const h = this._canvas.height / this._dpr;

    ctx.font = "16px sans-serif";
    ctx.textAlign = "left";
    ctx.textBaseline = "bottom";
    const padding = 6;
    const fontSize = 16;
    const lineHeight = fontSize * 1.2;

    // 最大文字幅取得
    let maxWidth = 0;
    for (const s of this._seriesList) {
      const m = ctx.measureText(s.name()).width;
      if (m > maxWidth) maxWidth = m;
    }

    // 枠サイズ
    const boxW = maxWidth + padding * 2;
    const boxH = this._seriesList.length * lineHeight + padding * 2;

    // 右下配置
    const x0 = w - boxW - 40;
    const y0 = h - boxH - 20;

    // 背景
    ctx.fillStyle = "#FFFB";
    ctx.fillRect(x0, y0, boxW, boxH);

    // テキスト
    for (let i = 0; i < this._seriesList.length; i++) {
      const s = this._seriesList[i];
      ctx.fillStyle = s.color();
      const ty = y0 + padding + i * lineHeight + lineHeight / 2;
      ctx.fillText(s.name(), x0 + padding, ty);
    }
  }

  render() {
    this.clear();
    this._drawAxes();

    for (const s of this._seriesList) {
      const n = s.x.length;

      // 線
      if (s.drawLines() && n > 1) {
        for (let i = 0; i < n - 1; i++) {
          this._drawLine(s.x[i], s.y[i], s.x[i+1], s.y[i+1], s.xAxis(), s.yAxis(), s.color(), 1);
        }
      }

      // 点
      if (s.drawPoints()) {
        for (let i = 0; i < n; i++) {
          this._drawPoint(s.x[i], s.y[i], s.xAxis(), s.yAxis(), 4, s.color());
        }
      }
    }

    this._drawLegend();
  }
}

window.Plot2D = Plot2D;