function loadPage(page){

fetch(page)

.then(res => res.text())

.then(html => {

document.getElementById("content").innerHTML = html

// chạy script nếu cần
if(page.includes("dashboard")){
loadDashboard()
}

})

}

// load trang đầu
loadPage("pages/home.html")