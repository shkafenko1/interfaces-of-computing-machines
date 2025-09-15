document.addEventListener('DOMContentLoaded', () => {
    const powerSourceEl = document.getElementById('powerSource');
    const batteryTypeEl = document.getElementById('batteryType');
    const batteryLevelEl = document.getElementById('batteryLevel');
    const fullRuntimeEl = document.getElementById('fullRuntime');
    const remainingTimeEl = document.getElementById('remainingTime');

    const cowImage = document.querySelector('.cow');
    const allButtons = document.querySelectorAll('.mc-button');
    let lastPowerSource = null;

    const normalCowSrc = '../../public/cow.gif';
    const chargeCowSrc = '../../public/chargecow.gif';
    
    const buttonSoundPath = '../../public/sounds/minecraft_click.mp3';
    const chargeSoundPath = '../../public/sounds/charge.mp3';
    const unplugSoundPath = '../../public/sounds/death2.wav';

    window.electronAPI.startCpp();

    const formatTime = (seconds, isFullRuntime = false) => {
        if (seconds === -1 || typeof seconds === 'undefined' || seconds === 4294967295) {
            return isFullRuntime ? 'N/A' : 'Calculating...';
        }
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        return `${h}h ${m}m`;
    };

    window.electronAPI.onCppData((data) => {
        try {
            const info = JSON.parse(data);

            powerSourceEl.textContent = info.powerSource;
            batteryTypeEl.textContent = info.batteryType || 'N/A';
            batteryLevelEl.textContent = `${info.batteryLevel}%`;
            fullRuntimeEl.textContent = formatTime(info.batteryFullLifeTime, true);
            remainingTimeEl.textContent = formatTime(info.batteryLifeTime);

            const newPowerSource = info.powerSource.trim();

            if (cowImage && newPowerSource !== lastPowerSource) {
                if (newPowerSource === 'Online') {
                    cowImage.src = chargeCowSrc;
                    new Audio(chargeSoundPath).play();
                } else {
                    cowImage.src = normalCowSrc;
                    new Audio(unplugSoundPath).play();
                }
            }
            lastPowerSource = newPowerSource;

        } catch (error) {
            console.error("Failed to parse JSON from main process:", data);
        }
    });

    if (allButtons.length > 0) {
        allButtons.forEach(button => {
            button.addEventListener('click', (event) => {
                const link = button.parentElement;

                if (link && link.tagName === 'A' && link.href) {
                    event.preventDefault();
                    const audio = new Audio(buttonSoundPath);
                    audio.play();
                    audio.addEventListener('ended', () => {
                        window.location.href = link.href;
                    });
                } else {
                    new Audio(buttonSoundPath).play();
                    if (button.id === 'sleepBtn') {
                        window.electronAPI.sendCommand('sleep');
                    } else if (button.id === 'hibernateBtn') {
                        window.electronAPI.sendCommand('hibernate');
                    }
                }
            });
        });
    }
});