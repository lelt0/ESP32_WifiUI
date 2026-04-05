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
    for (const [pAxis, p] of [[this._xAxis, x], [this._yAxis, y]]) {
      if(pAxis.autoScale()){
        if(pAxis.range() == 0){ pAxis.setMinMax(p-1e-12, p+1e-12); }
        else if(p > pAxis.max()){ pAxis.setMinMax(pAxis.min(), p); }
        else if(p < pAxis.min()){ pAxis.setMinMax(p, pAxis.max()); }
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
class Plot {
  constructor(canvas, heightRatio=0.8) {
    this._canvas = canvas;
    this._ctx = canvas.getContext("2d");
    this._dpr = window.devicePixelRatio || 1;
    this._viewHeightRatio = heightRatio;

    this._seriesList = [];

    window.addEventListener("resize", () => this.resize());
  }
  
  // 高DPI対応リサイズ
  resize() {
    const parentWidth = this._canvas.parentElement.clientWidth;
    const maxHeight = window.innerHeight * 0.9; // Plot高さはウィンドウ高さの90%以下に制限

    let cssWidth  = parentWidth;
    let cssHeight  = cssWidth * this._viewHeightRatio;
    if (cssHeight > maxHeight) {
      cssWidth = maxHeight / this._viewHeightRatio;
      cssHeight = maxHeight;
    }
    this._canvas.style.width = cssWidth + 'px';
    this._canvas.style.height = cssHeight + 'px';
    this._canvas.width = cssWidth * this._dpr;
    this._canvas.height = cssHeight * this._dpr;
    this._ctx.setTransform(this._dpr, 0, 0, this._dpr, 0, 0);

    this.render();
  }
  
  getSeriesNameList() {
    const series_name_list = [];
    for(const s of this._seriesList) series_name_list.push(s.name());
    return series_name_list;
  }

  getSeries(name) {
    return this._seriesList.find(s => s.name() === name);
  }
  clearSeries() { this._seriesList = []; }
  
  clear() {
    const w = this._canvas.width / this._dpr;
    const h = this._canvas.height / this._dpr;
    this._ctx.clearRect(0, 0, w, h);
  }
}
class Plot2D extends Plot {
  constructor(canvas, heightRatio=0.8, xRange=[0, 0], yRange=[0, 0], xAxisName="X", yAxisName="Y") {
    super(canvas, heightRatio);

    this._xAxes = [new _Axis(xAxisName, xRange[0], xRange[1], -1)];
    this._yAxes = [new _Axis(yAxisName, yRange[0], yRange[1], -1)];
    this._xAxes[0].drawAt(this._yAxes[0]);
    this._yAxes[0].drawAt(this._xAxes[0]);

    this.resize();
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
  
  addSeries(name, color, xArray = null, yArray = null, drawPoints = true, drawLine = true, xAxis = this._xAxes[0], yAxis = this._yAxes[0]) {
    const s = new _DataSeries2D(name, color, xAxis, yAxis, drawPoints, drawLine);
    if (!yArray) yArray = [];
    if (!xArray) xArray = [...Array(yArray.length).keys()];
    s.setData(xArray, yArray);
    this._seriesList.push(s);
    return s;
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

// =========================
// DataSeries3D
// =========================
class _DataSeries3D {
  constructor(name, xAxis, yAxis, zAxis) {
    this._name = name;
    this._xAxis = xAxis;
    this._yAxis = yAxis;
    this._zAxis = zAxis;
    this._drawPoints = true;
    this._drawLines = false; // note: 予約

    this.x = [];
    this.y = [];
    this.z = [];
    this.colors = [];
  }

  name() { return this._name; }
  xAxis() { return this._xAxis; }
  yAxis() { return this._yAxis; }
  zAxis() { return this._zAxis; }
  drawPoints(b=null) { if(b!=null){ this._drawPoints = b; } return this._drawPoints; }
  drawLines(b=null) { if(b!=null){ this._drawLines = b; } return this._drawLines; }

  addData(x, y, z, color) {
    for (const [pAxis, p] of [[this._xAxis, x], [this._yAxis, y], [this._zAxis, z]]) {
      if(pAxis.autoScale()){
        if(pAxis.range() == 0){ pAxis.setMinMax(p-1e-12, p+1e-12); }
        else if(p > pAxis.max()){ pAxis.setMinMax(pAxis.min(), p); }
        else if(p < pAxis.min()){ pAxis.setMinMax(p, pAxis.max()); }
      }
    }
    this.x.push(x);
    this.y.push(y);
    this.z.push(z);
    this.colors.push(color);
  }

  setData(xArray, yArray, zArray, colorArray) {
    this.x = [];
    this.y = [];
    this.z = [];
    this.colors = [];
    for(let i = 0; i < xArray.length; i++) {
      this.addData(xArray[i], yArray[i], zArray[i], colorArray[i]);
    }
  }
}

// =========================
// Plot3D
// =========================
class Plot3D extends Plot {
  constructor(canvas, heightRatio=1.0, xRange=[0, 0], yRange=[0, 0], zRange=[0, 0], axisScale=[1.0, 1.0, 1.0]) {
    super(canvas, heightRatio);

    this._xAxis = new _Axis("X", xRange[0], xRange[1]);
    this._yAxis = new _Axis("Y", yRange[0], yRange[1]);
    this._zAxis = new _Axis("Z", zRange[0], zRange[1]);
    this._axisScale = axisScale;

    this._yaw = -Math.PI / 4;
    this._pitch = -Math.PI / 4;
    this._zoom = 0.9;
    this._panX = 0;
    this._panY = 0;

    this._resetView();
    this.resize();

    this._lastPointerX = null;
    this._lastPointerY = null;
    this._startZoomScale = null;
    const dxdy = (x,y) => {
        const dx = this._lastPointerX !== null ? x - this._lastPointerX : 0;
        const dy = this._lastPointerY !== null ? y - this._lastPointerY : 0;
        this._lastPointerX = x;
        this._lastPointerY = y;
        return [dx, dy];
    };
    const rotate = (d_yaw, d_pitch) => {
      this._yaw += d_yaw;
      this._pitch += d_pitch;
      const limit = Math.PI / 2 - 0.01;
      if (this._pitch > limit) this._pitch = limit;
      if (this._pitch < -limit) this._pitch = -limit;
    };
    const zoom = (scale) => {
      this._zoom = scale;
      this._zoom = Math.max(0.1, Math.min(10, this._zoom));
    };

    this._canvas.addEventListener("pointerdown", (e) => {
      if (e.pointerType === 'touch') return;
      e.preventDefault(); // ブラウザ機能を抑制
      this._canvas.setPointerCapture(e.pointerId); // マウスがcanvas外に出てもドラッグ継続
    });
    this._canvas.addEventListener("pointermove", (e) => {
      if (e.pointerType === 'touch') return;
      if (e.buttons) {
        e.preventDefault();
        const [dx, dy] = dxdy(e.clientX, e.clientY);
        if (e.buttons & 1) { rotate(dx * 0.01, -dy * 0.01); }
        if (e.buttons & 4) { this._panX += dx; this._panY += dy; }
        if (e.buttons & 2) { zoom(this._zoom * (dy > 0 || dx < 0 ? 0.98 : 1.02)); }
        this.render();
      }
    });
    this._canvas.addEventListener("pointerup", (e) => {
      if (e.pointerType === 'touch') return;
      e.preventDefault();
      dxdy(null, null);
      this._canvas.releasePointerCapture(e.pointerId);
    });
    // this._canvas.addEventListener("wheel", (e) => {
    //   e.preventDefault();
    //   zoom(this._zoom * (e.deltaY > 0 ? 0.9 : 1.1));
    //   this.render();
    // }, { passive: false });
    this._canvas.addEventListener("dblclick", () => { this._resetView(); this.render(); });
    this._canvas.addEventListener('touchmove', (e) => {
      if (e.touches.length == 1) {
        e.preventDefault();
        const [dx, dy] = dxdy(e.touches[0].clientX, e.touches[0].clientY);
        rotate(dx * 0.01, -dy * 0.01);
        this.render();
      }
      if (e.touches.length == 2) {
        e.preventDefault();
        const t1 = e.touches[0];
        const t2 = e.touches[1];
        const centerX = (t1.clientX + t2.clientX) / 2;
        const centerY = (t1.clientY + t2.clientY) / 2;
        const pointersDist = Math.hypot(t1.clientX - t2.clientX, t1.clientY - t2.clientY);
        if (this._startZoomScale == null) {
          dxdy(centerX, centerY);
          this._startZoomScale = this._zoom / pointersDist;
        }
        const [dx, dy] = dxdy(centerX, centerY);
        this._panX += dx; this._panY += dy;
        zoom(pointersDist * this._startZoomScale);
        this.render();
      }
    }, { passive: false });
    this._canvas.addEventListener('touchend', (e) => {
      dxdy(null, null);
      this._startZoomScale = null;
    });
    this._canvas.addEventListener("contextmenu", (e) => e.preventDefault());
  }

  _resetView() {
      this._yaw = -Math.PI / 4;
      this._pitch = -Math.PI / 4;
      this._zoom = 0.32;
      this._panX = 0;
      this._panY = 0;
  }

  addSeries(name, xArray, yArray, zArray, colorArray) {
    const s = new _DataSeries3D(name, this._xAxis, this._yAxis, this._zAxis);
    s.setData(xArray, yArray, zArray, colorArray);
    this._seriesList.push(s);
    return s;
  }

  _project(x, y, z) {
    const scaled_xrange = this._xAxis.range() * this._axisScale[0];
    const scaled_yrange = this._yAxis.range() * this._axisScale[1];
    const scaled_zrange = this._zAxis.range() * this._axisScale[2];
    const maxRange = Math.max(scaled_xrange, scaled_yrange, scaled_zrange, 1e-12);

    // 軸Box中心、最大の軸範囲が[-1,1]になるように正規化
    const nx = (2 * (x - this._xAxis.min()) - this._xAxis.range()) * this._axisScale[0] / maxRange;
    const ny = (2 * (y - this._yAxis.min()) - this._yAxis.range()) * this._axisScale[1] / maxRange;
    const nz = (2 * (z - this._zAxis.min()) - this._zAxis.range()) * this._axisScale[2] / maxRange;

    const cy = Math.cos(this._yaw);
    const sy = Math.sin(this._yaw);
    const cp = Math.cos(this._pitch);
    const sp = Math.sin(this._pitch);

    // yaw: Z軸周りに回転
    const x1 = cy * nx - sy * ny;
    const y1 = sy * nx + cy * ny;

    // pitch: X軸周りに回転
    const z2 = cp * nz - sp * y1;
    const y2 = sp * nz + cp * y1;

    return [x1, z2, y2];
  }

  _toScreenP(px, py) {
    const w = this._canvas.width / this._dpr;
    const h = this._canvas.height / this._dpr;

    const scale = Math.min(w, h) * this._zoom;
    const sx = w / 2 + this._panX + px * scale;
    const sy = h / 2 + this._panY - py * scale;

    return [sx, sy];
  }

  _drawLineP(px1, py1, px2, py2, color = "#888", width = 1) {
    const [sx1, sy1] = this._toScreenP(px1, py1);
    const [sx2, sy2] = this._toScreenP(px2, py2);

    this._ctx.beginPath();
    this._ctx.moveTo(sx1, sy1);
    this._ctx.lineTo(sx2, sy2);
    this._ctx.strokeStyle = color;
    this._ctx.lineWidth = width;
    this._ctx.stroke();
  }

  _drawPointP(px, py, r = 4, color = "red") {
    const [sx, sy] = this._toScreenP(px, py);

    this._ctx.beginPath();
    this._ctx.arc(sx, sy, r, 0, Math.PI * 2);
    this._ctx.fillStyle = color;
    this._ctx.fill();
  }

  _drawAxes(behind = false) {

    const originProj = this._project(this._xAxis.min(), this._yAxis.min(), this._zAxis.min());
    const drawAxesBehind = (originProj[2] < 0);
    if (!(behind ^ drawAxesBehind)) return;

    const xMaxProj = this._project(this._xAxis.max(), this._yAxis.min(), this._zAxis.min());
    const yMaxProj = this._project(this._xAxis.min(), this._yAxis.max(), this._zAxis.min());
    const zMaxProj = this._project(this._xAxis.min(), this._yAxis.min(), this._zAxis.max());
    const axes = [
      { axis: this._xAxis, color: "#D55", maxProj: xMaxProj },
      { axis: this._yAxis, color: "#5A5", maxProj: yMaxProj },
      { axis: this._zAxis, color: "#55D", maxProj: zMaxProj }
    ];

    const ctx = this._ctx;
    const axisColor = "#888";
    const labelColor = "#444";

    for (const a of axes) {
      this._drawLineP(originProj[0], originProj[1], a.maxProj[0], a.maxProj[1], axisColor, 2);

      const ticks = a.axis.createTicks();
      ctx.fillStyle = labelColor;
      ctx.font = "11px sans-serif";
      ctx.textAlign = "left";
      ctx.textBaseline = "middle";
      for (const t of ticks) {
        let tx, ty, tz;
        if (a.axis === this._xAxis) [tx, ty, tz] = [t, this._yAxis.min(), this._zAxis.min()];
        if (a.axis === this._yAxis) [tx, ty, tz] = [this._xAxis.min(), t, this._zAxis.min()];
        if (a.axis === this._zAxis) [tx, ty, tz] = [this._xAxis.min(), this._yAxis.min(), t];

        const pt = this._project(tx, ty, tz);
        this._drawPointP(pt[0], pt[1], 2, a.color);

        const [sx, sy] = this._toScreenP(pt[0], pt[1]);
        let label = Number(t.toFixed(12)).toString();
        if (label.length >= 7) label = Number(t).toExponential(2);
        ctx.fillText(label, sx + 6, sy - 6);
      }

      const [sx, sy] = this._toScreenP(a.maxProj[0], a.maxProj[1]);
      ctx.fillStyle = a.color;
      ctx.font = "bold 14px sans-serif";
      ctx.textAlign = "left";
      ctx.textBaseline = "middle";
      ctx.fillText(a.axis.name(), sx + 16, sy);
    }
  }

  render() {
    this.clear();
    this._drawAxes(true);

    const pointsProj = [];
    for (const s of this._seriesList) {
      if (!s.drawPoints()) continue;
      for (let i = 0; i < s.x.length; i++) {
        const [px, py, pz] = this._project(s.x[i], s.y[i], s.z[i]);
        pointsProj.push({ px: px, py: py, pz: pz, color: s.colors[i] });
      }
    }
    if (pointsProj.length > 0) {
      pointsProj.sort((a, b) => b.pz - a.pz);

      const min_pz = pointsProj.at(-1).pz;
      const max_pz = pointsProj.at(0).pz;
      for (const p of pointsProj) {
        this._drawPointP(p.px, p.py, 4 - 2 * (p.pz - min_pz) / (max_pz - min_pz), p.color);
      }
    }

    this._drawAxes(false);
  }
}
window.Plot3D = Plot3D;