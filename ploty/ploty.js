// =========================
// Axis
// =========================
class Axis {
  constructor(name, min = -10, max = 10, drawAt = "lowest") {
    this.name = name;
    this.min = min;
    this.max = max;
    this.drawAt = drawAt; // "lowest" or "highest" or AxisObject (means this Axis is drawn at '0' of AxisObject)
  }

  createTicks(targetCount = 10) {
    const range = this.max - this.min;
    if (range <= 0) return [];

    const roughStep = range / targetCount;
    const pow = Math.pow(10, Math.floor(Math.log10(roughStep)));
    const steps = [1, 2, 5].map(v => v * pow);
    const step = steps.reduce((prev, curr) =>
      Math.abs(curr - roughStep) < Math.abs(prev - roughStep) ? curr : prev
    );

    const start = Math.ceil(this.min / step) * step;
    const ticks = [];
    for (let v = start; v <= this.max; v += step) { ticks.push(v); }
    return ticks;
  }
}

// =========================
// DataSeries2D
// =========================
class DataSeries2D {
  constructor(name, color, xAxis, yAxis, drawPoints = true, drawLine = true) {
    this.name = name;
    this.color = color;
    this.drawPoints = drawPoints;
    this.drawLine = drawLine;
    this.xAxis = xAxis;
    this.yAxis = yAxis;

    this.x = [];
    this.y = [];
  }

  addData(x, y) {
    this.x.push(x);
    this.y.push(y);
  }

  setData(xArray, yArray) {
    this.x = xArray.slice();
    this.y = yArray.slice();
  }
}

// =========================
// Graph2D
// =========================
class Graph2D {
  constructor(canvas, heightRatio=0.8, yRange=[0, 1], xRange=[0, 10]) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");

    this.viewHeightRatio = heightRatio;
    this.xAxes = [new Axis("X0", xRange[0], xRange[1])];
    this.yAxes = [new Axis("Y0", yRange[0], yRange[1])];
    this.xAxes[0].drawAt = this.yAxes[0];
    this.yAxes[0].drawAt = this.xAxes[0];

    this.seriesList = [];

    this.resize();
    window.addEventListener("resize", () => this.resize());
  }

  getSeries(name) {
    return this.seriesList.find(s => s.name === name);
  }

  addSeries(series) {
    this.seriesList.push(series);
  }

  // 高DPI対応リサイズ
  resize() {
    const rect = this.canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;

    this.canvas.width = rect.width * dpr;
    this.canvas.height = rect.width * dpr * this.viewHeightRatio;

    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    this.render();
  }

  toScreen(x, y, xAxis = null, yAxis = null) {
    let sx = x;
    if (xAxis) {
      const w = this.canvas.width / (window.devicePixelRatio || 1);
      sx = (x - xAxis.min) / (xAxis.max - xAxis.min) * w;
    }

    let sy = y;
    if (yAxis) {
      const h = this.canvas.height / (window.devicePixelRatio || 1);
      sy = h - (y - yAxis.min) / (yAxis.max - yAxis.min) * h;
    }

    return [sx, sy];
  }

  clear() {
    const w = this.canvas.width / (window.devicePixelRatio || 1);
    const h = this.canvas.height / (window.devicePixelRatio || 1);
    this.ctx.clearRect(0, 0, w, h);
  }

  // TODO: データ数が多くなったら連続描画（polyline）を検討 
  drawLine(x1, y1, x2, y2, xAxis = null, yAxis = null, color = "#888", width = 1) {
    const [sx1, sy1] = this.toScreen(x1, y1, xAxis, yAxis);
    const [sx2, sy2] = this.toScreen(x2, y2, xAxis, yAxis);

    this.ctx.beginPath();
    this.ctx.moveTo(sx1, sy1);
    this.ctx.lineTo(sx2, sy2);
    this.ctx.strokeStyle = color;
    this.ctx.lineWidth = width;
    this.ctx.stroke();
  }

  drawPoint(x, y, xAxis = null, yAxis = null, r = 4, color = "red") {
    const [sx, sy] = this.toScreen(x, y, xAxis, yAxis);

    this.ctx.beginPath();
    this.ctx.arc(sx, sy, r, 0, Math.PI * 2);
    this.ctx.fillStyle = color;
    this.ctx.fill();
  }

  drawAxes() {
    const ctx = this.ctx;
    const dpr = window.devicePixelRatio || 1;
    const w = this.canvas.width / dpr;
    const h = this.canvas.height / dpr;

    // X軸の描画Y座標
    const xAxes_sy = [];
    for (const xAxis of this.xAxes)
    {
      const dummy_yAxis = ((xAxis.drawAt instanceof Axis) ? xAxis.drawAt : this.yAxes[0]);
      let xaxis_sy = this.toScreen(0, 
        xAxis.drawAt instanceof Axis ? 0 : (xAxis.drawAt === "highest" ? dummy_yAxis.max : dummy_yAxis.min), 
        xAxis, dummy_yAxis)[1];
      xaxis_sy = xaxis_sy < 0 ? 0 : (xaxis_sy > w ? w : xaxis_sy);
      xAxes_sy.push(xaxis_sy);
    }
    // Y軸の描画X座標
    const yAxes_sx = [];
    for (const yAxis of this.yAxes)
    {
      const dummy_xAxis = ((yAxis.drawAt instanceof Axis) ? yAxis.drawAt : this.xAxes[0]);
      let yaxis_sx = this.toScreen(
        yAxis.drawAt instanceof Axis ? 0 : (yAxis.drawAt === "highest"? dummy_xAxis.max : dummy_xAxis.min), 
        0, dummy_xAxis, yAxis)[0];
      yaxis_sx = yaxis_sx < 0 ? 0 : (yaxis_sx > h ? h : yaxis_sx);
      this.drawLine(yaxis_sx, 0, yaxis_sx, h, null, null, "#888", 2);
      yAxes_sx.push(yaxis_sx);
    }

    // X軸の目盛り線X座標
    const xAxes_ticks = []
    const xAxes_ticks_sx = []
    for (const xAxis of this.xAxes) {
        const xAxis_ticks = xAxis.createTicks()
        xAxes_ticks.push(xAxis_ticks)
        xAxes_ticks_sx.push(xAxis_ticks.map((t) => this.toScreen(t, 0, xAxis, null)[0]));
    }
    // Y軸の目盛り線Y座標
    const yAxes_ticks = []
    const yAxes_ticks_sy = []
    for (const yAxis of this.yAxes) {
        const yAxis_ticks = yAxis.createTicks()
        yAxes_ticks.push(yAxis_ticks)
        yAxes_ticks_sy.push(yAxis_ticks.map((t) => this.toScreen(0, t, null, yAxis)[1]));
    }

    // 目盛り線
    for (let i = 0; i < xAxes_ticks_sx.length; i++) {
        const sy = xAxes_sy[i];
        for (const sx of xAxes_ticks_sx[i]) {
          this.drawLine(sx, 0, sx, h, null, null, "#ccc", 1);
        }
    }
    for (let i = 0; i < yAxes_ticks_sy.length; i++) {
        const sx = yAxes_sx[i];
        for (const sy of yAxes_ticks_sy[i]) {
          this.drawLine(0, sy, w, sy, null, null, "#ccc", 1);
        }
    }

    // 軸
    for (const sy of xAxes_sy) this.drawLine(0, sy, w, sy, null, null, "#888", 2);
    for (const sx of yAxes_sx) this.drawLine(sx, 0, sx, h, null, null, "#888", 2);

    // 目盛りラベル
    const X_TICK_LABEL_MARGIN = 10
    for (let i = 0; i < xAxes_sy.length; i++) {
        const sy = xAxes_sy[i];
        for (let j = 0; j < xAxes_ticks[i].length; j++) {
            const t = xAxes_ticks[i][j];
            const sx = xAxes_ticks_sx[i][j];

            // 端すぎたりY軸と被るラベルは描画しない
            let skip = false;
            if (sx < X_TICK_LABEL_MARGIN) continue;
            if (sx > w - X_TICK_LABEL_MARGIN) continue;
            for (const yaxis_sx of yAxes_sx) {
              if (sx >= yaxis_sx - X_TICK_LABEL_MARGIN && sx <= yaxis_sx + X_TICK_LABEL_MARGIN) { skip = true; break; }
            }
            if (skip) continue;

            let t_ = Number(t.toFixed(12));
            let label = t_.toString(); if (label.length >= 7) label = t_.toExponential(2);
            ctx.fillStyle = "#444";
            ctx.font = "12px sans-serif";
            ctx.textAlign = "center";
            if (sy < h / 2) {
              ctx.textBaseline = "top";
              ctx.fillText(label, sx, sy + 6);
            } else {
              ctx.textBaseline = "bottom";
              ctx.fillText(label, sx, sy - 6);
            }
        }
    }
    const Y_TICK_LABEL_MARGIN = 6
    for (let i = 0; i < yAxes_sx.length; i++) {
        const sx = yAxes_sx[i];
        for (let j = 0; j < yAxes_ticks[i].length; j++) {
            const t = yAxes_ticks[i][j];
            const sy = yAxes_ticks_sy[i][j];
  
            // 端すぎたりY軸と被るラベルは描画しない
            let skip = false;
            if (sy < Y_TICK_LABEL_MARGIN) continue;
            if (sy > h - Y_TICK_LABEL_MARGIN) continue;
            for(const xaxis_sy of xAxes_sy) {
                if (sy >= xaxis_sy - Y_TICK_LABEL_MARGIN && sy <= xaxis_sy + Y_TICK_LABEL_MARGIN) { skip = true; break; }
            }
            if (skip) continue;
  
            let t_ = Number(t.toFixed(12));
            let label = t_.toString(); if (label.length >= 7) label = t_.toExponential(2);
            ctx.textBaseline = "middle";
            if (sx > w / 2) {
              ctx.textAlign = "right";
              ctx.fillText(label, sx - 6, sy);
            } else {
              ctx.textAlign = "left";
              ctx.fillText(label, sx + 6, sy);
            }
        }
    }
  }

  addData(name, x, y) {
    let s = this.getSeries(name);
    if (!s) {
      s = new DataSeries2D(name, "red", this.xAxes[0], this.yAxes[0], true, true);
      this.addSeries(s);
    }
    s.addData(x, y);
  }

  render() {
    this.clear();
    this.drawAxes();

    for (const s of this.seriesList) {
      const n = s.x.length;

      // 線
      if (s.drawLine && n > 1) {
        for (let i = 0; i < n - 1; i++) {
          this.drawLine(s.x[i], s.y[i], s.x[i+1], s.y[i+1], s.xAxis, s.yAxis, s.color, 1);
        }
      }

      // 点
      if (s.drawPoints) {
        for (let i = 0; i < n; i++) {
          this.drawPoint(s.x[i], s.y[i], s.xAxis, s.yAxis, 4, s.color);
        }
      }
    }
  }
}
