document.addEventListener('DOMContentLoaded', () => {
    const deviceListEl = document.getElementById('deviceList');
    const logConsoleEl = document.getElementById('logConsole');
    const statusMessageEl = document.getElementById('statusMessage');
    const refreshBtn = document.getElementById('refreshBtn');
    const clearLogBtn = document.getElementById('clearLogBtn');
    const buttonSoundPath = '../../public/sounds/minecraft_click.mp3';

    const cowWalk = document.getElementById('cow-walk');
    if (cowWalk) {
        cowWalk.style.left = '0px';
        let direction = 'right';
        let currentLeft = 0;
        const animateCow = () => {
            if (direction === 'right') {
                currentLeft += 0.5;
                if (currentLeft + 500 >= window.innerWidth) { direction = 'left'; cowWalk.style.transform = 'scaleX(-1)'; }
            } else {
                currentLeft -= 0.5;
                if (currentLeft <= 0) { direction = 'right'; cowWalk.style.transform = 'scaleX(1)'; }
            }
            cowWalk.style.left = currentLeft + 'px';
            requestAnimationFrame(animateCow);
        };
        animateCow();
    }

    function playClick() { new Audio(buttonSoundPath).play().catch(() => {}); }

    function showStatus(msg, variant = '') {
        statusMessageEl.textContent = msg;
        statusMessageEl.className = `status-message show ${variant}`;
        setTimeout(() => statusMessageEl.classList.remove('show'), 3000);
    }

    function addLog(message, type = 'normal') {
        const div = document.createElement('div');
        const time = new Date().toLocaleTimeString();
        div.className = `log-entry ${type}`;
        div.innerHTML = `<span class="time">[${time}]</span> ${message}`;
        logConsoleEl.appendChild(div);
        logConsoleEl.scrollTop = logConsoleEl.scrollHeight;
    }

    function renderDevices(devices) {
        deviceListEl.innerHTML = '';
        if (devices.length === 0) {
            deviceListEl.innerHTML = '<div class="loading-text">No USB devices detected</div>';
            return;
        }

        devices.forEach(dev => {
            const item = document.createElement('div');
            item.className = 'device-item';
            
            const safeId = dev.id.replace(/\\/g, '\\\\');
            
            let actions = '';
            if (dev.type === 'DISK') {
                const lockText = dev.isLocked ? 'Unlock' : 'Lock';
                const lockClass = dev.isLocked ? 'action-btn lock active' : 'action-btn lock';
                
                actions += `<button class="${lockClass}" title="Prevent Windows Safe Removal" onclick="toggleLock('${safeId}', ${dev.isLocked})">${lockText}</button>`;
                actions += `<button class="action-btn danger" onclick="ejectDevice('${safeId}')">Eject</button>`;
            } else {
                actions = '<span style="color:#777">-</span>';
            }

            const displayName = dev.name.replace(/\(.*\)/, ''); 

            item.innerHTML = `
                <span>${displayName} <br><small style="color:#aaa">${dev.path || ''}</small></span>
                <span>${dev.type}</span>
                <span>${dev.isLocked ? '<b style="color:red">LOCKED</b>' : 'Active'}</span>
                <div class="device-actions">${actions}</div>
            `;
            deviceListEl.appendChild(item);
        });
    }

    window.toggleLock = (id, currentState) => {
        playClick();
        const command = currentState ? 'unlock' : 'lock';
        sendCommand(`${command}|${id}`);
        if (!currentState) addLog("Locking device... Try ejecting via Taskbar to see refusal.");
    };

    window.ejectDevice = (id) => {
        playClick();
        sendCommand(`eject|${id}`);
        addLog(`Sending eject command...`);
    };

    function startCppProcess() {
        if (!window.electronAPI) {
            addLog('Error: Not running in Electron.', 'error');
            return;
        }

        window.electronAPI.startCpp('lab5');

        window.electronAPI.onCppData((data) => {
            try {
                const lines = data.split('\n').filter(line => line.trim() !== '');
                lines.forEach(line => {
                    const info = JSON.parse(line);
                    if (info.type === 'device_list') {
                        renderDevices(info.devices || []);
                    } else if (info.type === 'log') {
                        addLog(info.message, info.level || 'normal');
                        
                        if (info.message.includes("Inserted")) showStatus("Device Connected", "success");
                        if (info.message.includes("Removed")) showStatus("Device Removed", "error");
                        if (info.message.includes("Success")) showStatus("Safely Ejected", "success");
                        if (info.message.includes("DENIED")) showStatus("Safe Removal Refused", "error");
                    }
                });
            } catch (e) {
                console.error("JSON Error:", e);
            }
        });
    }

    function sendCommand(cmd) {
        if (window.electronAPI) window.electronAPI.sendCommand(cmd);
    }

    refreshBtn.addEventListener('click', () => {
        playClick();
        sendCommand('refresh');
    });

    clearLogBtn.addEventListener('click', () => {
        playClick();
        logConsoleEl.innerHTML = '';
    });

    startCppProcess();
});