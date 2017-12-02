
function startup() {
    var url = '/company/all';
    $.get(url, function(resp, stat){
        if (stat == 'success') {
            var source = document.getElementById("lists").innerHTML;
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
        title: '创建新企业',
        area: ['650px', '420px'],
        content: template
    });
}

function formSubmit() {
    var url = '/template/create';
    var form = new Object();

    form.rexp = $("input[name=rexp]").val();
    form.name = $("input[name=name]").val();
    form.contents = $("input[name=contents]").val();

    var request = new XMLHttpRequest();

    request.open("POST", "test.php");
    request.send(form);
}

function formChange() {
    var url = '/template/update';
    console.log($("#form").serializeArray());
}

function editCompany(id) {
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

function deleteCompany(id) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var url = '/company/delete?id=' + id;
        $.get(url, function(){
            layer.msg('删除成功!', {icon: 1, time: 1000});
            setTimeout(function() {
                window.location.reload();
            }, 1000);
        });
    });
}

function recharge(id) {
    layer.prompt({title: '请输入充值金额', formType: 0}, function(money, index){
        var url = '/company/recharge';
        $.post(url, {"id": id, "money": money}, function(response, status) {
            layer.close(index);
            if (status == 'success' && response.status == 200) {
                layer.msg('充值成功!', {icon: 1, time: 1000});
                setTimeout(function() {
                    window.location.reload();
                }, 1000);
            } else {
                layer.msg('充值失败: ' + response.message, {icon: 2, time: 5000});
            }
        });
    });
}
