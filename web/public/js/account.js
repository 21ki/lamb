
function startup() {
    var method = "GET";
    var url = '/api/accounts';
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("contents").innerHTML;
            Handlebars.registerHelper('checkcharge', function(type) {
                return type == 1 ? '提交计费' : '成功计费';
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
    var url = "/api/companys";
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = template = document.getElementById("new-page").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);
            layer.open({
                type: 1,
                title: '创建新帐号',
                area: ['880px', '520px'],
                content: contents
            });

            $("input[name=password]").val(genPassword(8));
            $("#security-options").multiselect({buttonWidth: '260px'});
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function formSubmit() {
    var method = "POST";
    var url = '/account/create';
    var form = document.getElementById("form");
    var options = 0;
    var selected = [];
    var data = new FormData(form);

    /* Get the selected options */
    $("#security-options option:selected").each(function(){
        selected.push($(this).val());
    });

    for (i in selected) {
        switch (selected[i]) {
        case 'template':
            options |= 1;
            break;
        case 'keyword':
            options |= 1 << 1;
            break;
        case 'frequency':
            options |= 1 << 2;
            break;
        case 'unsubscribe':
            options |= 1 << 3;
            break;
        case 'blacklist':
            options |= 1 << 4;
            break;
        }
    }

    data.append("options", options);

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
    var url = '/account/update?id=' + id;
    var form = document.getElementById("form");
    var options = 0;
    var selected = [];
    var data = new FormData(form);

    /* Get the selected options */
    $("#security-options option:selected").each(function(){
        selected.push($(this).val());
    });

    for (i in selected) {
        switch (selected[i]) {
        case 'template':
            options |= 1;
            break;
        case 'keyword':
            options |= 1 << 1;
            break;
        case 'frequency':
            options |= 1 << 2;
            break;
        case 'unsubscribe':
            options |= 1 << 3;
            break;
        case 'blacklist':
            options |= 1 << 4;
            break;
        }
    }

    data.append("options", options);

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

function editAccount(id) {
    var method = "GET";
    var url = '/api/account?id=' + id;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var data = new Object();
            data.account = xhr.response.data;
            url = "/api/companys";
            xhr.onreadystatechange = function(){
                if (xhr.readyState === xhr.DONE && xhr.status === 200) {
                    data.company = xhr.response.data;
                    var source = document.getElementById("edit-page").innerHTML;
                    Handlebars.registerHelper('checkselected', function(company) {
                        return (company == data.account.company) ? new Handlebars.SafeString('selected="selected"') : '';
                    });
                    Handlebars.registerHelper('checkchecked', checkChecked);
                    var template = Handlebars.compile(source);
                    var contents = template(data);
                    layer.open({
                        type: 1,
                        title: '编辑帐号信息',
                        area: ['880px', '520px'],
                        content: contents
                    });

                    var selected = checkOptions(data.account.options);
                    $("#security-options").multiselect({buttonWidth: '260px'});
                    $("#security-options").multiselect('select', selected);
                }
            }
            xhr.open(method, url, true);
            xhr.send();
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function checkOptions(options) {
    var selected = [];
    options = parseInt(options);

    if (options & 1) {
        selected.push("template");
    }

    if (options & (1 << 1)) {
        selected.push("keyword");
    }

    if (options & (1 << 2)) {
        selected.push("frequency");
    }

    if (options & (1 << 3)) {
        selected.push("unsubscribe");
    }

    if (options & (1 << 4)) {
        selected.push("blacklist");
    }

    return selected;
}

function deleteAccount(id) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = "DELETE";
        var url = '/account/delete?id=' + id;
        var xhr = new XMLHttpRequest();
        xhr.responseType = "json";
        xhr.onreadystatechange = function(){
            if (xhr.readyState === xhr.DONE && xhr.status === 200) {
                if (xhr.response.status === 200) {
                    layer.msg('删除成功!', {icon: 1, time: 1000});
                } else {
                    layer.msg('删除失败!', {icon: 2, time: 1000});
                }

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

function genPassword(length, chars) { 
    var len = length;
    
    if (!chars) {
        var chars = "abcdefghkmnpqrstuvwxyz23456789";
    }

    var password = ""; 
    
    for(var x = 0; x < len; x++) { 
        var i = Math.floor(Math.random() * chars.length); 
        password += chars.charAt(i); 
    } 
    
    return password;
} 

function checkSelected(val1, val2){
    return (val1 == val2) ? new Handlebars.SafeString('selected="selected"') : '';
}

function checkChecked(val) {
    return (val == 1) ? new Handlebars.SafeString('checked="checked"') : '';
}
