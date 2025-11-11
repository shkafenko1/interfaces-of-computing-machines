async function runXP() {
    console.log("Pretending to run XP script...");
    return Promise.resolve();
}

async function readDiskData() {
    try {
        const response = await fetch('file:///D:/studies/interfaces/shared xp/disk_info.json');
        if (!response.ok) {
            throw new Error("Could not fetch disk_info.json. The file may not exist yet.");
        }
        return await response.json();
    } catch (error) {
        console.error(error);
        return [];
    }
}

function renderDisks(disks) {
    const container = document.getElementById('disks-container');
    container.innerHTML = '';

    const fakeDataDB = [
        { disk_id: 0, model: 'VBOX HARDDISK', serial_number: 'VB1a2b3c4d-5e6f7g8h', firmware_version: '1.0', override_title: null },
        { disk_id: 1, model: 'VBOX CD-ROM', serial_number: 'N/A', firmware_version: '1.2', override_title: 'VirtualBox Guest Additions' },
        { disk_id: 2, model: 'VIRTUAL CD/DVD-ROM', serial_number: 'N/A', firmware_version: '1.0', override_title: 'VMware Tools' }
    ];

    disks.forEach(disk => {
        const fakeData = fakeDataDB.find(data => data.disk_id === disk.disk_id);
        if (fakeData) {
            if (!disk.model) disk.model = fakeData.model;
            if (!disk.serial_number) disk.serial_number = fakeData.serial_number;
            if (!disk.firmware_version) disk.firmware_version = fakeData.firmware_version;
            disk.display_title = fakeData.override_title || disk.model;
        } else {
            disk.display_title = disk.model || 'Unknown Model';
        }
    });

    if (!disks || disks.length === 0) {
        container.innerHTML = '<div class="loading">No disks found or error reading file.</div>';
        return;
    }

    disks.forEach(disk => {
        const card = document.createElement('div');
        card.className = 'disk-card';
        const serial = disk.serial_number || 'N/A';
        const firmware = disk.firmware_version || 'N/A';
        const isCDROM = disk.display_title.includes('Guest Additions') || disk.display_title.includes('Tools');
        const iconSrc = isCDROM ? '../../public/hdd.png' : 
                      (disk.disk_type === 'SSD' ? '../../public/ssd.png' : '../../public/hdd.png');

        const partition = disk.partition_info || { total_gb: 0, used_gb: 0 };
        const usedPercent = partition.total_gb > 0 ? (partition.used_gb / partition.total_gb) * 100 : 0;
        
        card.innerHTML = `
            <div class="disk-header">
                <img src="${iconSrc}" alt="${disk.disk_type}" class="disk-icon">
                <div class="disk-title">${disk.display_title}</div>
            </div>
            <div class="disk-info">
                <div class="info-item"><span class="info-label">Model:</span><span class="info-value">${disk.model}</span></div>
                <div class="info-item"><span class="info-label">Serial:</span><span class="info-value">${serial}</span></div>
                <div class="info-item"><span class="info-label">Firmware:</span><span class="info-value">${firmware}</span></div>
                <div class="info-item"><span class="info-label">Position:</span><span class="info-value">Disk ${disk.disk_id} (${disk.position})</span></div>
            </div>
            <div>
                <div class="partition-bar"><div class="partition-used" style="width: ${usedPercent.toFixed(2)}%;"></div></div>
                <div class="partition-text">${partition.path.slice(0, 2)} Used: ${partition.used_gb.toFixed(2)} GB / ${partition.total_gb.toFixed(2)} GB</div>
            </div>
            <div class="features-container">${disk.supported_features.map(feature => `<div class="feature-tag">${feature}</div>`).join('')}</div>
        `;
        container.appendChild(card);
    });
}

const cowAnimation = {
    isPaused: false,
    direction: 'right',
    speed: 0.7,
    animationFrameId: null,

    start() {
        const cowContainer = document.getElementById('cow-container');
        const loadingGif = document.getElementById('cow-loading');
        cowContainer.style.left = '0px';
        loadingGif.classList.add('right');
        this.animate();
    },

    animate() {
        if (this.isPaused) {
            this.animationFrameId = requestAnimationFrame(() => this.animate());
            return;
        }

        const cowContainer = document.getElementById('cow-container');
        const cowWalk = document.getElementById('cow-walk');
        const loadingGif = document.getElementById('cow-loading');
        
        const cowRect = cowContainer.getBoundingClientRect();
        let currentLeft = parseFloat(cowContainer.style.left);

        if (this.direction === 'right') {
            currentLeft += this.speed;
            if (currentLeft + cowRect.width >= window.innerWidth) {
                this.direction = 'left';
                cowWalk.style.transform = 'scaleX(-1)';
                loadingGif.classList.remove('right');
                loadingGif.classList.add('left');
            }
        } else {
            currentLeft -= this.speed;
            if (currentLeft <= 0) {
                this.direction = 'right';
                cowWalk.style.transform = 'scaleX(1)';
                loadingGif.classList.remove('left');
                loadingGif.classList.add('right');
            }
        }

        cowContainer.style.left = currentLeft + 'px';
        this.animationFrameId = requestAnimationFrame(() => this.animate());
    },

    pauseFor(duration) {
        this.isPaused = true;
        setTimeout(() => {
            this.isPaused = false;
        }, duration);
    }
};

async function loadAndDisplayData(withAnimation = false) {
    const container = document.getElementById('disks-container');
    const loadingGif = document.getElementById('cow-loading');

    if (withAnimation) {
        cowAnimation.pauseFor(2000);
        loadingGif.classList.remove('hidden');
    }
    
    container.innerHTML = '<div class="loading">Loading data...</div>';

    try {
        const disks = await readDiskData();
        
        if (withAnimation) {
            setTimeout(() => {
                renderDisks(disks);
                loadingGif.classList.add('hidden');
            }, 2000);
        } else {
            renderDisks(disks);
        }
    } catch (error) {
        console.error(error.message);
        container.innerHTML = `<div class="loading">Error loading data: ${error.message}</div>`;
        if (withAnimation) {
            loadingGif.classList.add('hidden');
        }
    }
}

async function handleRunXP() {
    const runButton = document.getElementById('run-xp-button');
    runButton.disabled = true;
    runButton.textContent = 'Running...';
    
    try {
        await runXP();
        setTimeout(() => {
            loadAndDisplayData(false);
        }, 2000); 
    } catch (error) {
        console.error("Failed to run XP script:", error);
    } finally {
        setTimeout(() => {
            runButton.disabled = false;
            runButton.textContent = 'Run XP';
        }, 2000);
    }
}

document.addEventListener('DOMContentLoaded', () => {
    cowAnimation.start();
    loadAndDisplayData(false);
    
    document.getElementById('run-xp-button').addEventListener('click', handleRunXP);
    document.getElementById('refresh-button').addEventListener('click', () => loadAndDisplayData(true));
});