
function startup() {
    var method = "GET";
    var url = '/api/gateways';
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
                $(this).delay(i * 25).fadeIn(200);
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function show() {
    var template = document.getElementById("new-page").innerHTML;
    layer.open({
        type: 1,
        title: '创建新网关',
        area: ['915px', '620px'],
        content: template
    });

    $('input[type=checkbox]').iCheck({
        checkboxClass: 'icheckbox_flat-blue',
        radioClass: 'iradio_flat-blue',
        increaseArea: '20%'
    });
}

function formSubmit() {
    var method = "POST";
    var url = '/gateway/create';
    var form = document.getElementById("new-form");
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
    var url = '/gateway/update?id=' + id;
    var form = document.getElementById("edit-form");
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

function changeGateway(id) {
    var method = "GET";
    var url = '/api/gateway?id=' + id;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("edit-page").innerHTML;
            Handlebars.registerHelper('checkselectd', function(val1, val2) {
                return (val1 == val2) ? new Handlebars.SafeString('selected="selected"') : '';
            });
            Handlebars.registerHelper('checkchecked', function(val) {
                return (val == 1) ? new Handlebars.SafeString('checked="checked"') : '';
            });
            var template = Handlebars.compile(source);
            var contents = template(xhr.response.data);
            layer.open({
                type: 1,
                title: '编辑网关信息',
                area: ['915px', '620px'],
                content: contents
            });

            $('input[type=checkbox]').iCheck({
                checkboxClass: 'icheckbox_flat-blue',
                radioClass: 'iradio_flat-blue',
                increaseArea: '20%'
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function deleteGateway(id) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = "DELETE";
        var url = '/gateway/delete?id=' + id;
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

function checkGateway(id, name) {
    var source = document.getElementById("check-page").innerHTML;
    var template = Handlebars.compile(source);
    var contents = template({id: id});

    layer.open({
        type: 1,
        title: '通道测试 (' + name + ')',
        area: ['600px', '390px'],
        content: contents
    });
}

function formQuery() {
    layer.load(2, {
        shade: [0.1,'#cccccc']
    });
    var method = "GET";
    var where = $("#where").serialize();
    var url = "/gateway/getreport?" + where;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            var source = document.getElementById("contents").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);

            $(".contents tbody").empty();
            $(".contents tbody").append(contents);
            $(".contents tbody tr").hide();
            $(".contents tbody tr").each(function(i){
                $(this).delay(i * 25).fadeIn(200);
            });
        }
    }

    xhr.open(method, url, true);
    xhr.send();
}

function getdateofday(time) {
    var now = new Date();
    var month = now.getMonth() + 1;
    var day = now.getDate(); 

    if (month >= 1 && month <= 9) {
        month += "0";
    }

    if (day >= 1 && day <= 9) {
        day += "0";
    }
    
    return now.getFullYear() + '-' + month + '-' + day + " " + time;
}
