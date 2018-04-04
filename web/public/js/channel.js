
function startup() {
    var method = "GET";
    var id = getQuery("id");
    var url = '/api/channels?acc=' + id;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("contents").innerHTML;
            Handlebars.registerHelper('checkoperator', checkoperator);
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
                area: ['460px', '280px'],
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

    var operator = 0;
    data.append("acc", getQuery("id"));

    if (data.has("cmcc")) {
        operator |= 1;
        data.delete("cmcc");
    }

    if (data.has("ctcc")) {
        operator |= (1 << 1);
        data.delete("ctcc");
    }

    if (data.has("cucc")) {
        operator |= (1 << 2);
        data.delete("cucc");
    }

    if (data.has("mvno")) {
        operator |= (1 << 3);
        data.delete("mvno");
    }

    data.append("operator", operator);

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

function formChange(id, acc) {
    var method = "POST";
    var url = '/channels/update?id=' + id + '&acc=' + acc;
    var form = document.getElementById("form");
    var data = new FormData(form);

    var operator = 0;

    if (data.has("cmcc")) {
        operator |= 1;
        data.delete("cmcc");
    }

    if (data.has("ctcc")) {
        operator |= (1 << 1);
        data.delete("ctcc");
    }

    if (data.has("cucc")) {
        operator |= (1 << 2);
        data.delete("cucc");
    }

    if (data.has("mvno")) {
        operator |= (1 << 3);
        data.delete("mvno");
    }

    data.append("operator", operator);

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

function changeChannel(id, acc) {
    var method = "GET";
    var url = '/api/channel?id=' + id + '&acc=' + acc;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("edit-page").innerHTML;
            Handlebars.registerHelper('checked', checked);
            var template = Handlebars.compile(source);
            var contents = template(xhr.response.data);
            layer.open({
                type: 1,
                title: '编辑通道信息',
                area: ['460px', '280px'],
                content: contents
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function deleteChannel(id, acc) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = "DELETE";
        var url = '/channels/delete?id=' + id + '&acc=' + acc;
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

function checked(oper, val) {
    if (oper & (1 << parseInt(val))) {
        return new Handlebars.SafeString('checked="checked"');
    }
}

function checkoperator(val) {
    var text = '';
    val = parseInt(val);

    if (val & 1) {
        text += '<span class="label label-cmcc">移动</span> ';
    }

    if (val & (1 << 1)) {
        text += '<span class="label label-ctcc">电信</span> ';
    }

    if (val & (1 << 2)) {
        text += '<span class="label label-cucc">联通</span> ';
    }

    if (val & (1 << 3)) {
        text += '<span class="label label-other">其他</span>';
    }

    return new Handlebars.SafeString(text);
}
