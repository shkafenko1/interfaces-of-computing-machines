const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const { spawn } = require('child_process');

app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');

let cppProcess = null;

const createWindow = () => {
  const win = new BrowserWindow({
    width: 800,
    height: 600,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: false
    }
  });

  ipcMain.on('start-cpp', () => {
    let exePath;
    
    if (app.isPackaged) {
      exePath = path.join(process.resourcesPath, 'main.exe');
    } else {
      exePath = path.join(__dirname, 'src', 'lab1', 'main.exe');
    }

    cppProcess = spawn(exePath);
    let buffer = '';

    cppProcess.stdout.on('data', (data) => {
      buffer += data.toString();
      let boundary = buffer.indexOf('\n');
      while (boundary !== -1) {
        const jsonString = buffer.substring(0, boundary);
        buffer = buffer.substring(boundary + 1);
        if (jsonString) {
            win.webContents.send('cpp-data', jsonString);
        }
        boundary = buffer.indexOf('\n');
      }
    });

    cppProcess.stderr.on('data', (data) => {
      console.error(`C++ STDERR: ${data}`);
    });

    cppProcess.on('close', (code) => {
      console.log(`C++ process exited with code ${code}`);
      cppProcess = null;
    });
  });

  ipcMain.on('send-command-to-cpp', (event, command) => {
    if (cppProcess) {
      cppProcess.stdin.write(`${command}\n`);
    }
  });
  
  win.loadFile('src/startup/index.html');
};

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});