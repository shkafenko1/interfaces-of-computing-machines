async function runXP() {
    return window.electronAPI.runXP();
}

async function readPCIData() {
    try {
        await runXP();
        const response = await fetch('D:\\studies\\interfaces\\shared xp\\pci_devices.txt');
        if (!response.ok) {
            throw new Error("no file(");
        }
        const rawPCIData = await response.text();
        const PCIJSON = JSON.parse(rawPCIData);
        const PCIData = PCIJSON.devices;
        return PCIData;
    } catch (error) {
        console.error(error);
        return [];
    }
}

function createPCITable(devices) {
    const tbody = document.getElementById('dev-body');
    if (devices.length === 0) {
        tbody.innerHTML = '<tr><td colspan="2" class="loading">No devices found.</td></tr>';
        return;
    }
    const rows = devices.map(device => `
        <tr>
            <td>${device.DeviceID}</td>
            <td>${device.VendorID}</td>
        </tr>
    `).join('');
    tbody.innerHTML = rows;
}

document.addEventListener('DOMContentLoaded', async () => {
    const cowWalk = document.getElementById('cow-walk');
    const cowStill = document.getElementById('cow-still');
    const cowContainer = document.querySelector('.cow-container');

    let animationFrameId;
    let direction = 'right';
    const speed = 10;

    const animateCow = () => {
        const cowRect = cowWalk.getBoundingClientRect();
        let currentLeft = cowRect.left;

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
        animationFrameId = requestAnimationFrame(animateCow);
    };

    cowWalk.style.left = '0px';
    cowWalk.style.display = 'block';
    animateCow();

    try {
        const devices = await readPCIData();
        createPCITable(devices);
    } catch (error) {
        console.error(error.message);
    } finally {
        cancelAnimationFrame(animationFrameId);
        cowWalk.style.transform = 'scaleX(1)';

        const containerRect = cowContainer.getBoundingClientRect();
        const stillCowRect = cowStill.getBoundingClientRect();

        cowWalk.style.left = (containerRect.left + (containerRect.width - stillCowRect.width) / 2) + 'px';
        cowWalk.style.bottom = (window.innerHeight - containerRect.bottom + (containerRect.height - stillCowRect.height) / 2) + 'px';

        setTimeout(() => {
            cowWalk.style.display = 'none';
            cowStill.style.visibility = 'visible';
        }, 500);
    }
});