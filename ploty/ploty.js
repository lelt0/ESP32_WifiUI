// =========================
// Axis
// =========================
class Axis {
  constructor(name, min = -10, max = 10) {
    this.name = name;
    this.min = min;
    this.max = max;
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
  constructor(canvas, heightRatio=0.8) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");

    this.viewHeightRatio = heightRatio;
    this.xAxes = [new Axis("X0", -10, 10)];
    this.yAxes = [new Axis("Y0", -10, 10)];

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

  toScreen(x, y, xAxis, yAxis) {
    const w = this.canvas.width / (window.devicePixelRatio || 1);
    const h = this.canvas.height / (window.devicePixelRatio || 1);

    const sx = (x - xAxis.min) / (xAxis.max - xAxis.min) * w;
    const sy = h - (y - yAxis.min) / (yAxis.max - yAxis.min) * h;
    return [sx, sy];
  }

  clear() {
    const w = this.canvas.width / (window.devicePixelRatio || 1);
    const h = this.canvas.height / (window.devicePixelRatio || 1);
    this.ctx.clearRect(0, 0, w, h);
  }

  // TODO: データ数が多くなったら連続描画（polyline）を検討 
  drawLine(x1, y1, x2, y2, xAxis, yAxis, color = "#888", width = 1) {
    const [sx1, sy1] = this.toScreen(x1, y1, xAxis, yAxis);
    const [sx2, sy2] = this.toScreen(x2, y2, xAxis, yAxis);

    this.ctx.beginPath();
    this.ctx.moveTo(sx1, sy1);
    this.ctx.lineTo(sx2, sy2);
    this.ctx.strokeStyle = color;
    this.ctx.lineWidth = width;
    this.ctx.stroke();
  }

  drawPoint(x, y, xAxis, yAxis, r = 4, color = "red") {
    const [sx, sy] = this.toScreen(x, y, xAxis, yAxis);

    this.ctx.beginPath();
    this.ctx.arc(sx, sy, r, 0, Math.PI * 2);
    this.ctx.fillStyle = color;
    this.ctx.fill();
  }

  drawAxes() {
    for (let i = 0; i < this.xAxes.length; i++) {
      this.drawLine(this.xAxes[i].min, 0, this.xAxes[i].max, 0, this.xAxes[i], this.yAxes[i]);
      this.drawLine(0, this.yAxes[i].min, 0, this.yAxes[i].max, this.xAxes[i], this.yAxes[i]);
    }
  }

  drawTicks() {
    const ctx = this.ctx;
    const dpr = window.devicePixelRatio || 1;
    const w = this.canvas.width / dpr;
    const h = this.canvas.height / dpr;

    ctx.strokeStyle = "#ccc";
    ctx.fillStyle = "#444";
    ctx.lineWidth = 1;
    ctx.font = "12px sans-serif";

    // X軸
    for (const xAxis of this.xAxes) {
      const ticks = xAxis.createTicks();
      for (const t of ticks) {
        if (t == 0) continue;

        const [sx, sy] = this.toScreen(t, 0, xAxis, this.yAxes[0]);

        // 目盛り線
        ctx.beginPath();
        ctx.moveTo(sx, 0);
        ctx.lineTo(sx, h);
        ctx.stroke();

        // ラベル
        let label = t.toString();
        if (label.length >= 7) label = t.toExponential(2);

        ctx.textAlign = "center";
        ctx.textBaseline = "bottom";
        ctx.fillText(label, sx, sy - 6);
      }
    }

    // Y軸
    for (const yAxis of this.yAxes) {
      const ticks = yAxis.createTicks();
      for (const t of ticks) {
        if (t == 0) continue;

        const [sx, sy] = this.toScreen(0, t, this.xAxes[0], yAxis);

        // 目盛り線
        ctx.beginPath();
        ctx.moveTo(0, sy);
        ctx.lineTo(w, sy);
        ctx.stroke();

        // ラベル
        let label = t.toString();
        if (label.length >= 7) label = t.toExponential(2);

        ctx.textAlign = "left";
        ctx.textBaseline = "middle";
        ctx.fillText(label, sx + 6, sy);
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
    this.drawTicks();

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
