
function startup() {
    var method = "GET";
    var gid = getQuery("gid");
    var url = '/api/channels?gid=' + gid;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("contents").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);
            $("tbody").append(contents);
            $("tbody tr").hide();
            $("tbody tr").each(function(i){
                $(this).delay(i * 25).fadeIn(300);
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function show() {
    var method = "GET";
    var url = "/api/gateways";
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("new-page").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);
            layer.open({
                type: 1,
                title: '添加新通道',
                area: ['460px', '260px'],
                content: contents
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function formSubmit() {
    var method = "POST";
    var url = '/channels/create';
    var form = document.getElementById("form");
    var data = new FormData(form);
    data.append("gid", getQuery("gid"));
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            $("tbody").empty();
            startup();
        }
    }
    xhr.open(method, url, true);
    xhr.send(data);
}

function formChange(id, gid) {
    var method = "POST";
    var url = '/channels/update?id=' + id + '&gid=' + gid;
    var form = document.getElementById("form");
    var data = new FormData(form);
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            $("tbody").empty();
            startup();
        }
    }
    xhr.open(method, url, true);
    xhr.send(data);
}

function changeChannel(id, gid) {
    var method = "GET";
    var url = '/api/channel?id=' + id + '&gid=' + gid;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("edit-page").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response.data);
            layer.open({
                type: 1,
                title: '编辑通道信息',
                area: ['460px', '260px'],
                content: contents
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function deleteChannel(id, gid) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = "DELETE";
        var url = '/channels/delete?id=' + id + '&gid=' + gid;
        var xhr = new XMLHttpRequest();
        xhr.responseType = "json";
        xhr.onreadystatechange = function(){
            if (xhr.readyState === xhr.DONE && xhr.status === 200) {
                layer.msg('删除成功!', {icon: 1, time: 1000});
                setTimeout(function() {
                    $("tbody").empty();
                    startup();
                }, 1000);
            }
        }
        xhr.open(method, url, true);
        xhr.send();
    });
}

function getQuery(name) {
    var rexp = new RegExp("(^|&)" + name + "=([^&]*)(&|$)");
    var r = window.location.search.substr(1).match(rexp);
    return  (r != null) ? unescape(r[2]) : '';
}
