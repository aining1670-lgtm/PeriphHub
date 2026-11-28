import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

/**
 * WebView2 loads the embedded bundle from file:///, where <script type="module" crossorigin>
 * fails opaque-origin CORS checks. Convert the inlined module script into a classic <script>
 * placed at the end of <body> so #root exists when it executes.
 */
function classicScriptForFileProtocol(): Plugin {
  return {
    name: 'classic-script-for-file-protocol',
    apply: 'build',
    enforce: 'post',
    transformIndexHtml(html) {
      let extractedScript = '';
      const stripped = html.replace(
        /<script\b([^>]*)\btype="module"([^>]*)>([\s\S]*?)<\/script>/i,
        (_match, before: string, after: string, body: string) => {
          const attrs = `${before}${after}`.replace(/\s*\bcrossorigin(?:="[^"]*")?/gi, '');
          extractedScript = `<script${attrs}>${body}</script>`;
          return '';
        },
      );
      if (!extractedScript) return html;
      const bodyCloseIdx = stripped.lastIndexOf('</body>');
      if (bodyCloseIdx < 0) return `${stripped}\n${extractedScript}`;
      return `${stripped.slice(0, bodyCloseIdx)}  ${extractedScript}\n  ${stripped.slice(bodyCloseIdx)}`;
    },
  };
}

export default defineConfig({
  plugins: [react(), tailwindcss(), viteSingleFile(), classicScriptForFileProtocol()],
  base: './',
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    assetsInlineLimit: 100_000_000,
    cssCodeSplit: false,
    reportCompressedSize: true,
    rollupOptions: {
      output: {
        format: 'iife',
        inlineDynamicImports: true,
      },
    },
  },
});
