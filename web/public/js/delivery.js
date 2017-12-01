
function show() {
    var template = document.getElementById("new-page").innerHTML;
    layer.open({
        type: 1,
        title: '添加下行路由',
        area: ['435px', '285px'],
        content: template
    });

    var url = '/api/accounts';
    $.get(url, function(resp, stat){
        if (stat == 'success') {
            var el = resp.data;

            for (i in el) {
                var option = '<option value="' + el[i].id + '">' + el[i].username + '</option>';
                $(option).appendTo("#accounts");
            }

        }
    });
}

function changeDelivery(id) {
    var template = document.getElementById("edit-page").innerHTML;

    layer.open({
        type: 1,
        title: '下行路由编辑',
        area: ['435px', '285px'],
        content: template
    });

    var url = '/api/accounts';
    $.get(url, function(resp, stat){
        if (stat == 'success') {
            var el = resp.data;

            for (i in el) {
                var option = '<option value="' + el[i].id + '">' + el[id].username + '</option>';
                $(option).appendTo("#accounts");
            }

        }
    });
}

function deleteDelivery(id) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var url = '/delivery/delete?id=' + id;
        $.get(url, function(){
            layer.msg('删除成功!', {icon: 1, time: 1000});
            setTimeout(function() {
                window.location.reload();
            }, 1000);
        });
    });
}
