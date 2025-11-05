async function runXP() {
    return window.electronAPI.runXP();
}

async function readPCIData() {
    try {
        const response = await fetch('D:\\studies\\interfaces\\shared xp\\pci_devices.json');
        if (!response.ok) {
            throw new Error("Could not fetch pci_devices.json. The file may not exist yet.");
        }
        const pciJson = await response.json();
        return pciJson.devices;
    } catch (error) {
        console.error(error);
        return [];
    }
}

function createPCITable(devices) {
    const table = document.getElementById('dev-table');
    const thead = table.querySelector('thead');
    const tbody = table.querySelector('tbody');

    thead.innerHTML = '';
    tbody.innerHTML = '';

    if (!devices || devices.length === 0) {
        tbody.innerHTML = '<tr><td colspan="1" class="loading">No devices found.</td></tr>';
        return;
    }

    const headers = Object.keys(devices[0]);
    
    const headerRow = document.createElement('tr');
    headers.forEach(headerText => {
        const th = document.createElement('th');
        th.textContent = headerText;
        headerRow.appendChild(th);
    });
    thead.appendChild(headerRow);

    const rows = devices.map(device => {
        const tr = document.createElement('tr');
        headers.forEach(header => {
            const td = document.createElement('td');
            td.textContent = device[header] !== undefined ? device[header] : 'N/A';
            tr.appendChild(td);
        });
        return tr;
    });

    rows.forEach(row => tbody.appendChild(row));
}

async function loadAndDisplayData() {
    const tbody = document.getElementById('dev-body');
    const thead = document.getElementById('dev-head');

    thead.innerHTML = '';
    tbody.innerHTML = '<tr><td colspan="1" class="loading">Refreshing devices data...</td></tr>';

    try {
        const devices = await readPCIData();
        createPCITable(devices);
    } catch (error) {
        console.error(error.message);
        tbody.innerHTML = `<tr><td colspan="1" class="loading">Error loading data: ${error.message}</td></tr>`;
    }
}

async function handleRunXP() {
    const runButton = document.getElementById('run-xp-button');
    runButton.disabled = true;
    runButton.textContent = 'Running...';
    
    try {
        await runXP();
        await loadAndDisplayData();
    } catch (error) {
        console.error("Failed to run XP script:", error);
    } finally {
        runButton.disabled = false;
        runButton.textContent = 'Run XP';
    }
}

function startCowAnimation() {
    const cowWalk = document.getElementById('cow-walk');
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

document.addEventListener('DOMContentLoaded', () => {
    startCowAnimation();

    loadAndDisplayData();
    
    document.getElementById('run-xp-button').addEventListener('click', handleRunXP);
    document.getElementById('refresh-button').addEventListener('click', loadAndDisplayData);
});