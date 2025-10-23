const { contextBridge, ipcRenderer } = require('electron');
const { exec } = require('child_process');
const path = require('path');

contextBridge.exposeInMainWorld('electronAPI', {
  startCpp: () => ipcRenderer.send('start-cpp'),
  sendCommand: (command) => ipcRenderer.send('send-command-to-cpp', command),
  onCppData: (callback) => ipcRenderer.on('cpp-data', (event, data) => callback(data)),
  runXP: () => {
    return new Promise((resolve, reject) => {
      const batPath = path.join(__dirname, 'src', 'lab2', 'pci.bat');
        exec(batPath, (error) => {
            if (error) {
                reject(error)
            }
            resolve()
        })
    })
}
});