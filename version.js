async function fetchVersion() {
    const response = await fetch('https://raw.githubusercontent.com/BubTec/HeatControl/main/version.txt');
    const version = await response.text();
    document.getElementById('version').textContent = version;
}
fetchVersion(); 
