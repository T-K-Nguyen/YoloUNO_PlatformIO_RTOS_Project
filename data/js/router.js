async function loadPage(page){

    const res = await fetch(page)

    const html = await res.text()

    document.getElementById("content").innerHTML = html

}

window.onload = () => {
    loadPage("dashboard.html")
}