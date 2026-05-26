# AskDocs Landing Page

Marketing site for AskDocs — terminal-native code editor with an AI tutor.

## Stack

Pure **HTML + CSS**. No build step, no JavaScript, no npm.

90's internet aesthetic with AskDocs phosphor green & amber colors.

## Local preview

Open `index.html` in a browser, or serve the folder:

```bash
cd landingpage
python3 -m http.server 8080
```

Visit [http://localhost:8080](http://localhost:8080).

## Screenshots

Add PNG captures to `screenshots/`:

| File | Used for |
|------|----------|
| `hero.png` | Hero section |
| `editor.png` | Editor pane preview |
| `tutor.png` | AI tutor chat |
| `explorer.png` | File explorer / @ mentions |

Replace placeholder divs in `index.html` with `<img>` tags when files exist.

## Pages

| File | Page |
|------|------|
| `index.html` | Home |
| `privacy.html` | Privacy Policy |
| `data-collection.html` | Data Collection |
| `cookies.html` | Cookie Policy |
| `terms.html` | Terms of Service |

## Deploy

Upload the `landingpage/` folder to any static host (GitHub Pages, Netlify, S3, etc.). No SPA fallback needed — each page is a real HTML file.
