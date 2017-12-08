
function startup() {
    var method = "GET";
    var url = '/api/keywords';
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

function show(tag) {
    var method = "GET";
    var url = "/keyword/tags";
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var data = new Object();
            data.tags = xhr.response;
            data.def = (typeof(tag) !== "undefined") ? tag : '';
            var source = document.getElementById("new-page").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(data);

            layer.open({
                type: 1,
                title: '添加关键词',
                area: ['540px', '260px'],
                content: contents
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function fetchKeywords(tag) {
    var method = "GET";
    var url = "/keyword/getkeywords?tag=" + tag;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = source = document.getElementById("contents").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);

            $("#elements").append(contents);
            $(".label").hide();
            $(".label").each(function(i){
                $(this).delay(i * 15).fadeIn(300);
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function formElement() {
    var method = "POST";
    var url = '/keyword/create';
    var form = document.getElementById("form");
    var data = new FormData(form);
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            window.location.reload();
        }
    }
    xhr.open(method, url, true);
    xhr.send(data);
}

function formSubmit() {
    var method = "POST";
    var url = '/keyword/create';
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
    var url = '/template/update?id=' + id;
    var form = document.getElementById("form");
    var data = new FormData(form);
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            $("#datalist").empty();
            startup();
        }
    }
    xhr.open(method, url, true);
    xhr.send(data);
}

function changeKeywords(tag) {
    var method = "GET";
    var url = '/api/keyword?tag=' + tag;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("edit-page").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response.data);
            layer.open({
                type: 1,
                title: '编辑关键字',
                area: ['650px', '350px'],
                content: contents
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function deleteKeyword(id, val) {
    layer.confirm('确定要将 "' + val + '" 关键词删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = 'DELETE';
        var url = '/keyword/delete?id=' + id;
        var xhr = new XMLHttpRequest();
        xhr.responseType = "json";
        xhr.onreadystatechange = function(){
            if (xhr.readyState === xhr.DONE && xhr.status === 200) {
                layer.msg('删除成功!', {icon: 1, time: 1000});
                window.location.reload();
            }
        }
        xhr.open(method, url);
        xhr.send();
    });
}

function deleteKeywords(tag) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = 'DELETE';
        var url = '/keyword/delete?tag=' + tag;
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
        xhr.open(method, url);
        xhr.send();
    });
}
