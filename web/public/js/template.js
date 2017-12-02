
function startup() {
    var url = '/api/templates';
    $.get(url, function(resp, stat){
        if (stat == 'success') {
            var source = document.getElementById("contents").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(resp);
            $("#datalist").append(contents);
            $("tbody tr").hide();
            $("tbody tr").each(function(i){
                $(this).delay(i * 30).fadeIn(500);
            });
        }
    });
}

function show() {
    var template = document.getElementById("new-page").innerHTML;
    layer.open({
        type: 1,
        title: '新建签名模板',
        area: ['550px', '360px'],
        content: template
    });
}

function formSubmit() {
    var method = "POST";
    var url = '/template/create';
    var form = document.getElementById("form");
    var data = new FormData(form);
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            $("#datalist").empty();
            startup();
        }
    }
    xhr.open(method, url, true);
    xhr.send(data);
}

function formChange() {
    var method = "POST";
    var url = '/template/update?id=' + $("input[name=id]").val();
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

function editTemplate(id) {
    var template = document.getElementById("edit-page").innerHTML;

    layer.open({
        type: 1,
        title: '签名模板',
        area: ['550px', '360px'],
        content: template
    });

    var url = '/api/template?id=' + id;
    $.get(url, function(resp, stat){
        if (stat == 'success') {
            var el = resp.data;
            $("input[name=id]").val(el.id);
            $("input[name=rexp]").val(el.rexp);
            $("input[name=name]").val(el.name);
            $("textarea[name=contents]").val(el.contents);
        }
    });
}

function deleteTemplate(id) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = 'DELETE';
        var url = '/template/delete?id=' + id;
        var xhr = new XMLHttpRequest();
        xhr.responseType = "json";
        xhr.onreadystatechange = function(){
            if (xhr.readyState === xhr.DONE && xhr.status === 200) {
                layer.msg('删除成功!', {icon: 1, time: 1000});
                setTimeout(function() {
                    $("#datalist").empty();
                    startup();
                }, 1000);
            }
        }
        xhr.open(method, url);
        xhr.send();
    });
}
