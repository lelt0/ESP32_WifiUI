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

    // X軸の描画Y座標, Y軸の描画X座標
    const xAxes_sy = [];
    const yAxes_sx = [];
    for(const [for_xaxis, pAxes, pAxes_sq, HorW] of [[1, this._xAxes, xAxes_sy, h], [0, this._yAxes, yAxes_sx, w]]) {
      for (const pAxis of pAxes)
      {
        let pAxis_sq;
        if (pAxis.drawAt() instanceof _Axis) {
          pAxis_sq = this._toScreen(0, 0, for_xaxis?null:pAxis.drawAt(), for_xaxis?pAxis.drawAt():null)[for_xaxis];
        } else if (pAxis.drawAt() < 0) {
          pAxis_sq = HorW + pAxis.drawAt();
        } else {
          pAxis_sq = pAxis.drawAt();
        }
        pAxis_sq = pAxis_sq < 0 ? 0 : (pAxis_sq > HorW ? HorW : pAxis_sq);
        pAxes_sq.push(pAxis_sq);
      }
    }

    // X軸の目盛り線X座標, Y軸の目盛り線Y座標
    const xAxes_ticks = []
    const xAxes_ticks_sx = []
    const yAxes_ticks = []
    const yAxes_ticks_sy = []
    for (const [for_xaxis, pAxes, pAxes_ticks, pAxes_ticks_sp] of [
               [1, this._xAxes, xAxes_ticks, xAxes_ticks_sx], 
               [0, this._yAxes, yAxes_ticks, yAxes_ticks_sy]]) {
      for (const pAxis of pAxes) {
        const pAxis_ticks = pAxis.createTicks()
        pAxes_ticks.push(pAxis_ticks)
        pAxes_ticks_sp.push(pAxis_ticks.map((t) => 
          for_xaxis? this._toScreen(t, 0, pAxis, null)[0] : this._toScreen(0, t, null, pAxis)[1]
        ));
      }
    }

    // 目盛り線
    for (const [for_x, pAxes, pAxes_sq, pAxes_ticks_sp, HorW] of [
               [1, this._xAxes, xAxes_sy, xAxes_ticks_sx, h], 
               [0, this._yAxes, yAxes_sx, yAxes_ticks_sy, w]]){
      for (let i = 0; i < pAxes_ticks_sp.length; i++) {
          const sq = pAxes_sq[i];
          const tickLength = pAxes[i].tickLength();
          const [q0, q1] = (tickLength < 0 ? [0, HorW] : [sq - tickLength, sq + tickLength]);
          for (const sp of pAxes_ticks_sp[i]) this._drawLine(for_x?sp:q0, for_x?q0:sp, for_x?sp:q1, for_x?q1:sp, null, null, "#ccc", 1);
      }
    }

    // 軸
    for (const sy of xAxes_sy) this._drawLine(0, sy, w, sy, null, null, "#888", 2);
    for (const sx of yAxes_sx) this._drawLine(sx, 0, sx, h, null, null, "#888", 2);

    // ラベル
    ctx.fillStyle = "#444";
    const X_TICK_LABEL_MARGIN = 5;
    const Y_TICK_LABEL_MARGIN = 6;
    for (const [for_xaxis, pAxes, pAxes_sq, qAxes_sp, pAxes_ticks, pAxes_ticks_sp, LABEL_MARGIN, HorW] of [
               [1, this._xAxes, xAxes_sy, yAxes_sx, xAxes_ticks, xAxes_ticks_sx, X_TICK_LABEL_MARGIN, h], 
               [0, this._yAxes, yAxes_sx, xAxes_sy, yAxes_ticks, yAxes_ticks_sy, Y_TICK_LABEL_MARGIN, w]]) {
      for (let i = 0; i < pAxes_sq.length; i++) {
        const pAxis = pAxes[i];
        const sq = pAxes_sq[i];
        const label_sq = (sq < HorW / 2 ? sq + 6 : sq - 6);
        if (for_xaxis)
          ctx.textBaseline = (sq < h / 2 ? "top" : "bottom");
        else
          ctx.textAlign = (sq > w / 2 ? "right" : "left");
        
        // 軸ラベル
        let AxisNameXminOrYmax = for_xaxis? w : 0;
        if ( pAxis.drawName() ) {
            ctx.font = "bold 12px sans-serif";
            if (for_xaxis) {
              ctx.textAlign = "left";
              AxisNameXminOrYmax = w - (5 + ctx.measureText(pAxis.name()).width + 5);
              ctx.fillText(pAxis.name(), AxisNameXminOrYmax + 5, label_sq);
            } else {
              ctx.textBaseline = "bottom";
              const m = ctx.measureText(pAxis.name());
              AxisNameXminOrYmax = 5 + (m.fontBoundingBoxAscent + m.fontBoundingBoxDescent) + 5;
              ctx.fillText(pAxis.name(), label_sq, AxisNameXminOrYmax - 5);
            }
        }

        // 目盛りラベル
        ctx.font = "12px sans-serif";
        if (for_xaxis)
          ctx.textAlign = "center";
        else
          ctx.textBaseline = "middle";
        for (let j = 0; j < pAxes_ticks[i].length; j++) {
            const t = pAxes_ticks[i][j];
            const sp = pAxes_ticks_sp[i][j];

            // 端すぎたり、Y軸や軸ラベルと被る目盛りラベルは描画しない
            let skip = false;
            if (sp < (for_xaxis? 0 : AxisNameXminOrYmax) + LABEL_MARGIN) continue;
            if (sp > (for_xaxis? AxisNameXminOrYmax : h) - LABEL_MARGIN) continue;
            for (const qaxis_sp of qAxes_sp) {
              if (sp >= qaxis_sp - LABEL_MARGIN && sp <= qaxis_sp + LABEL_MARGIN) { skip = true; break; }
            }
            if (skip) continue;

            let t_ = Number(t.toFixed(12));
            let label = t_.toString(); if (label.length >= 7) label = t_.toExponential(2);
            ctx.fillText(label, for_xaxis?sp:label_sq, for_xaxis?label_sq:sp);
        }
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