document.addEventListener('DOMContentLoaded', () => {
    const cameraNameEl = document.getElementById('cameraName');
    const cameraStatusEl = document.getElementById('cameraStatus');
    const cameraResolutionEl = document.getElementById('cameraResolution');
    const cameraFPSEl = document.getElementById('cameraFPS');
    const statusMessageEl = document.getElementById('statusMessage');

    const capturePhotoBtn = document.getElementById('capturePhotoBtn');
    const captureVideoBtn = document.getElementById('captureVideoBtn');
    const hiddenPhotoBtn = document.getElementById('hiddenPhotoBtn');
    const hiddenVideoBtn = document.getElementById('hiddenVideoBtn');
    const refreshBtn = document.getElementById('refreshBtn');

    const allButtons = document.querySelectorAll('.mc-button');
    const buttonSoundPath = '../../public/sounds/minecraft_click.mp3';

    function showStatus(message, isError = false, isSuccess = false) {
        statusMessageEl.textContent = message;
        statusMessageEl.className = 'status-message show';
        if (isError) {
            statusMessageEl.classList.add('error');
        } else if (isSuccess) {
            statusMessageEl.classList.add('success');
        }
        setTimeout(() => {
            statusMessageEl.classList.remove('show');
        }, 3000);
    }

    function startCppProcess() {
        if (window.electronAPI) {
            window.electronAPI.startCpp('lab4');

            window.electronAPI.onCppData((data) => {
                try {
                    const info = JSON.parse(data);
                    
                    if (info.type === 'camera_info') {
                        cameraNameEl.textContent = info.name || 'N/A';
                        cameraStatusEl.textContent = info.status || 'Unknown';
                        cameraResolutionEl.textContent = info.resolution || 'N/A';
                        cameraFPSEl.textContent = info.fps || 'N/A';
                    } else if (info.type === 'status') {
                        if (info.message) {
                            showStatus(info.message, info.error, info.success);
                        }
                    } else if (info.type === 'file_saved') {
                        showStatus(`File saved: ${info.filename}`, false, true);
                    }
                } catch (error) {
                    console.error("Failed to parse JSON from C++:", error);
                }
            });
        }
    }

    function sendCommand(command) {
        if (window.electronAPI) {
            window.electronAPI.sendCommand(command);
        }
    }

    capturePhotoBtn.addEventListener('click', () => {
        new Audio(buttonSoundPath).play();
        sendCommand('capture_photo');
        showStatus('Capturing photo...');
    });

    captureVideoBtn.addEventListener('click', () => {
        new Audio(buttonSoundPath).play();
        sendCommand('capture_video');
        showStatus('Capturing video...');
    });

    hiddenPhotoBtn.addEventListener('click', () => {
        new Audio(buttonSoundPath).play();
        sendCommand('hidden_photo');
        showStatus('Hidden photo sequence initiated...');
    });

    hiddenVideoBtn.addEventListener('click', () => {
        new Audio(buttonSoundPath).play();
        sendCommand('hidden_video');
        showStatus('Hidden video sequence initiated...');
    });

    refreshBtn.addEventListener('click', () => {
        new Audio(buttonSoundPath).play();
        sendCommand('refresh_info');
        showStatus('Refreshing camera info...');
    });

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
            }
        });
    });

    function startCowAnimation() {
        const cowWalk = document.getElementById('cow-walk');
        if (!cowWalk) return;

        cowWalk.style.left = '0px';
        let direction = 'right';
        const speed = 0.7;

        const animateCow = () => {
            const cowRect = cowWalk.getBoundingClientRect();
            let currentLeft = parseFloat(cowWalk.style.left);

            if (direction === 'right') {
                currentLeft += speed;
                if (currentLeft + cowRect.width >= window.innerWidth) {
                    direction = 'left';
                    cowWalk.style.transform = 'scaleX(-1)';
                }
            } else {
                currentLeft -= speed;
                if (currentLeft <= 0) {
                    direction = 'right';
                    cowWalk.style.transform = 'scaleX(1)';
                }
            }

            cowWalk.style.left = currentLeft + 'px';
            requestAnimationFrame(animateCow);
        };
        animateCow();
    }

    startCowAnimation();
    startCppProcess();
    
    setTimeout(() => {
        sendCommand('refresh_info');
    }, 500);
});