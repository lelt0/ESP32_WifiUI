import { build } from "esbuild";
import { minify } from "terser";
import { mkdirSync, readFileSync, writeFileSync, copyFileSync } from "fs";
import { gzipSync } from "zlib";

mkdirSync("dist", { recursive: true });

// まず軽く整形・minify
await build({
  entryPoints: ["src/ploty.js"],
  outfile: "dist/ploty.esbuild.js",
  bundle: true,
  format: "iife",
  minify: true,
  keepNames: true,
  target: ["es2020"]
});

// esbuild結果をさらにTerserでproperty mangle
const input = readFileSync("dist/ploty.esbuild.js", "utf8");

const terserConfig = JSON.parse(
  readFileSync("terser.config.json", "utf8")
);

const result = await minify(input, terserConfig);

writeFileSync("dist/ploty.min.js", result.code);

// gzip化
const gz = gzipSync(result.code, { level: 9 });
writeFileSync("dist/ploty.min.js.gz", gz);
