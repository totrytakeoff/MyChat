import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    strictPort: false,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:8102',
        changeOrigin: true,
      },
      '/ws': {
        target: 'wss://127.0.0.1:8101',
        ws: true,
        secure: false,
        changeOrigin: true,
      },
    },
  },
});
