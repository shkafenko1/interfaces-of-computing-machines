const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  startCpp: () => ipcRenderer.send('start-cpp'),
  sendCommand: (command) => ipcRenderer.send('send-command-to-cpp', command),
  onCppData: (callback) => ipcRenderer.on('cpp-data', (event, data) => callback(data))
});