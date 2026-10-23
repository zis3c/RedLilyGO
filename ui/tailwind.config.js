/** @type {import('tailwindcss').Config} */
module.exports = {
  darkMode: 'class',
  content: ['./content/index.html', './src/**/*.js', './src/**/*.html'],
  theme: {
    extend: {
      fontFamily: { sans: ['Inter', 'system-ui', 'sans-serif'] },
      colors: {
        primary: { 50: '#fef2f2', 100: '#fee2e2', 400: '#f87171', 500: '#ef4444', 600: '#dc2626', 900: '#7f1d1d' },
        surface: { light: '#ffffff', dark: '#18181b' },
        border: { light: '#e5e7eb', dark: '#27272a' }
      }
    }
  }
};
