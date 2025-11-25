const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const { spawn } = require('child_process');

app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');

let cppProcess = null;

const EXECUTABLES = {
  default: {
    dev: path.join(__dirname, 'src', 'lab1', 'main.exe'),
    prod: path.join(process.resourcesPath, 'main.exe')
  },
  lab1: {
    dev: path.join(__dirname, 'src', 'lab1', 'main.exe'),
    prod: path.join(process.resourcesPath, 'main.exe')
  },
  lab4: {
    dev: path.join(__dirname, 'src', 'lab4', 'webcam.exe'),
    prod: path.join(process.resourcesPath, 'webcam.exe')
  },
  lab5: {
    dev: path.join(__dirname, 'src', 'lab5', 'lab5.exe'),
    prod: path.join(process.resourcesPath, 'lab5.exe')
  }
};

const resolveExecutable = (labNumber) => {
  const entry = EXECUTABLES[labNumber] || EXECUTABLES.default;
  return app.isPackaged ? entry.prod : entry.dev;
};

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

  const sendProcessError = (message) => {
    if (win && win.webContents) {
      win.webContents.send('cpp-data', JSON.stringify({
        type: 'log',
        level: 'error',
        message
      }));
    }
  };

  ipcMain.on('start-cpp', (_event, labNumber) => {
    if (cppProcess) {
      cppProcess.kill();
      cppProcess = null;
    }

    const exePath = resolveExecutable(labNumber);

    try {
      cppProcess = spawn(exePath);
    } catch (error) {
      cppProcess = null;
      const logMessage = `Failed to launch helper (${labNumber || 'lab1'}): ${error.message}`;
      console.error(logMessage);
      sendProcessError(logMessage);
      return;
    }
    let buffer = '';

    cppProcess.on('error', (error) => {
      console.error(`C++ process error: ${error.message}`);
      sendProcessError(`Helper error: ${error.message}`);
      cppProcess = null;
    });

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