async function loadSidebar(){

const response = await fetch("components/sidebar.html")
const data = await response.text()

document.getElementById("sidebar").innerHTML = data

}

loadSidebar()