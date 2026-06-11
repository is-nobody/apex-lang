const express = require('express');
const multer = require('multer');
const path = require('path');
const fs = require('fs');
const db = require('./db');

const app = express();
app.use(express.json());
app.use(express.static('public')); // Serve the HTML frontend

// Ensure uploads directory exists
const uploadDir = './uploads';
if (!fs.existsSync(uploadDir)) {
    fs.mkdirSync(uploadDir);
}

// Multer setup for handling .zip uploads
const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        cb(null, uploadDir);
    },
    filename: (req, file, cb) => {
        // Use package name and timestamp to avoid conflicts
        const pkgName = req.body.pkg_name || 'package';
        cb(null, `${pkgName}_${Date.now()}.zip`);
    }
});
const upload = multer({ storage });

// --- API ENDPOINTS ---

// 1. Check if user exists (for registration validation)
app.get('/api/users/:username', (req, res) => {
    const user = db.prepare('SELECT * FROM users WHERE username = ?').get(req.params.username);
    res.status(user ? 200 : 404).json({ exists: !!user });
});

// 2. Register new user
app.post('/api/auth/register', (req, res) => {
    const { username, password } = req.body;
    if (!username || !password) return res.status(400).json({ error: 'Missing credentials' });
    
    try {
        db.prepare('INSERT INTO users (username, password) VALUES (?, ?)').run(username, password);
        res.status(201).json({ message: 'User registered successfully' });
    } catch (err) {
        res.status(409).json({ error: 'Username already taken' });
    }
});

// 3. Login user (Verify Password)
app.post('/api/auth/login', (req, res) => {
    const { username, password } = req.body;
    if (!username || !password) return res.status(400).json({ error: 'Missing credentials' });

    // Check both username AND password in DB
    const user = db.prepare('SELECT * FROM users WHERE username = ? AND password = ?').get(username, password);
    
    if (user) {
        res.json({ message: 'Login successful', username: user.username });
    } else {
        res.status(401).json({ error: 'Invalid username or password' });
    }
});

// 4. List all packages (for the website frontend)
app.get('/api/packages', (req, res) => {
    const pkgs = db.prepare('SELECT name, created_at FROM packages ORDER BY created_at DESC').all();
    res.json(pkgs);
});

// 5. Check if package exists (for `apex install`)
app.get('/api/packages/:name', (req, res) => {
    const pkg = db.prepare('SELECT * FROM packages WHERE name = ?').get(req.params.name);
    res.status(pkg ? 200 : 404).json(pkg || { error: 'Package not found' });
});

// 6. Download package zip (for `apex install`)
app.get('/api/packages/:name/download', (req, res) => {
    const pkg = db.prepare('SELECT * FROM packages WHERE name = ?').get(req.params.name);
    if (!pkg) return res.status(404).send('Package not found');
    
    if (fs.existsSync(pkg.file_path)) {
        res.download(pkg.file_path, `${req.params.name}.zip`);
    } else {
        res.status(500).send('File missing on server');
    }
});

// 7. Publish package (for `apex publish`)
app.post('/api/packages/publish', upload.single('file'), (req, res) => {
    const { username, pkg_name } = req.body;
    const file = req.file;
    
    if (!username || !file || !pkg_name) {
        return res.status(400).json({ error: 'Missing username, package name, or file' });
    }
    
    const user = db.prepare('SELECT * FROM users WHERE username = ?').get(username);
    if (!user) return res.status(404).json({ error: 'User not found' });
    
    try {
        db.prepare('INSERT INTO packages (name, owner_id, file_path) VALUES (?, ?, ?)')
          .run(pkg_name, user.id, file.path);
        res.status(201).json({ message: 'Package published successfully' });
    } catch (err) {
        res.status(409).json({ error: 'Package name already exists' });
    }
});

const PORT = 3000;
app.listen(PORT, '0.0.0.0', () => {
    console.log(`🚀 Apex Package Registry running on http://localhost:${PORT}`);
});