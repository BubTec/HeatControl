async function fetchVersion() {
    const response = await fetch('https://raw.githubusercontent.com/n3roGit/HeatControl/main/HeatController/version.txt');
    const version = await response.text();
    document.getElementById('version').textContent = version;
}
fetchVersion(); 