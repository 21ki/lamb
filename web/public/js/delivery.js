
function startup() {
    var method = "GET";
    var url = '/api/deliverys';
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("contents").innerHTML;
            Handlebars.registerHelper('seq', function(id) {
                return id + 1;
            });
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);
            $("tbody").append(contents);
            $("tbody tr").hide();
            $("tbody tr").each(function(i){
                $(this).delay(i * 25).fadeIn(200);
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function show() {
    var method = "GET";
    var url = "/api/accounts";
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("new-page").innerHTML;
            var template = Handlebars.compile(source);
            var contents =  template(xhr.response);
            layer.open({
                type: 1,
                title: '添加下行路由',
                area: ['500px', '280px'],
                content: contents
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function changeDelivery(id) {
    var method = "GET";
    var url = "/api/delivery?id=" + id;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var data = new Object();
            data.delivery = xhr.response.data;
            url = "/api/accounts";
            xhr.onreadystatechange = function(){
                if (xhr.readyState === xhr.DONE && xhr.status === 200) {
                    data.accounts = xhr.response.data;
                    var source = document.getElementById("edit-page").innerHTML;
                    Handlebars.registerHelper('checkselectd', function(val) {
                        return (val == data.delivery.target) ? new Handlebars.SafeString('selected="selected"') : '';
                    });
                    var template = Handlebars.compile(source);
                    var contents = template(data);

                    layer.open({
                        type: 1,
                        title: '下行路由编辑',
                        area: ['500px', '280px'],
                        content: contents
                    });
                }
            }
            xhr.open(method, url, true);
            xhr.send();
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function deleteDelivery(id) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = "DELETE";
        var url = "/delivery/delete?id=" + id;
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

function formSubmit() {
    var method = "POST";
    var url = '/delivery/create';
    var form = document.getElementById("form");
    var data = new FormData(form);
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

function formChange(id) {
    var method = "POST";
    var url = '/delivery/update?id=' + id;
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

function move(type, id) {
    var method = "GET";
    var url = '';

    if (type == 'up') {
        url = '/delivery/up?id=' + id;
    } else if (type == 'down') {
        url = '/delivery/down?id=' + id;
    } else {
        return;
    }

    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            $("tbody").empty();
            startup();
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

