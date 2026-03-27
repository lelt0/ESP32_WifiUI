// =========================
// DataSeries2D
// =========================
class DataSeries2D {
  constructor(name, color, drawPoints = true, drawLine = true) {
    this.name = name;
    this.color = color;
    this.drawPoints = drawPoints;
    this.drawLine = drawLine;

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
  constructor(canvas, height_ratio=0.8) {
    this.canvas = canvas;
    this.ctx = canvas.getContext("2d");

    this.viewHeightRatio = height_ratio;
    this.xMin = -10;
    this.xMax = 10;
    this.yMin = -10;
    this.yMax = 10;

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

  toScreen(x, y) {
    const w = this.canvas.width / (window.devicePixelRatio || 1);
    const h = this.canvas.height / (window.devicePixelRatio || 1);

    const sx = (x - this.xMin) / (this.xMax - this.xMin) * w;
    const sy = h - (y - this.yMin) / (this.yMax - this.yMin) * h;
    return [sx, sy];
  }

  clear() {
    const w = this.canvas.width / (window.devicePixelRatio || 1);
    const h = this.canvas.height / (window.devicePixelRatio || 1);
    this.ctx.clearRect(0, 0, w, h);
  }

  drawLine(x1, y1, x2, y2, color = "#888", width = 1) {
    const [sx1, sy1] = this.toScreen(x1, y1);
    const [sx2, sy2] = this.toScreen(x2, y2);

    this.ctx.beginPath();
    this.ctx.moveTo(sx1, sy1);
    this.ctx.lineTo(sx2, sy2);
    this.ctx.strokeStyle = color;
    this.ctx.lineWidth = width;
    this.ctx.stroke();
  }

  drawPoint(x, y, r = 4, color = "red") {
    const [sx, sy] = this.toScreen(x, y);

    this.ctx.beginPath();
    this.ctx.arc(sx, sy, r, 0, Math.PI * 2);
    this.ctx.fillStyle = color;
    this.ctx.fill();
  }

  drawAxes() {
    this.drawLine(this.xMin, 0, this.xMax, 0);
    this.drawLine(0, this.yMin, 0, this.yMax);
  }

  addData(name, x, y) {
    let s = this.getSeries(name);
    if (!s) {
      s = new DataSeries2D(name, "red", true, true);
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
          this.drawLine(s.x[i], s.y[i], s.x[i+1], s.y[i+1], s.color, 1);
        }
      }

      // 点
      if (s.drawPoints) {
        for (let i = 0; i < n; i++) {
          this.drawPoint(s.x[i], s.y[i], 4, s.color);
        }
      }
    }
  }
}
