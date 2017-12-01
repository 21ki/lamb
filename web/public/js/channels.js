function show(gid) {
    var template = document.getElementById("new-page").innerHTML;

    layer.open({
        type: 1,
        title: '添加通道',
        area: ['435px', '285px'],
        content: template
    });

    var url = "/api/gateways";
    $.get(url, function(resp, stat){
        if (stat == 'success') {
            var el = resp.data;

            $("input[name=gid]").val(gid);

            for (i in el) {
                var option = '<option value="' + el[i].id + '">' + el[i].name + '</option>';
                $(option).appendTo("#gateways");
            }
        }
    });
}

function editChannel(id, gid, name) {
    var template = document.getElementById("edit-page").innerHTML;

    layer.open({
        type: 1,
        title: '编辑通道',
        area: ['435px', '285px'],
        content: template
    });

    var url = '/api/channel?id=' + id + '&gid=' + gid;
    $.get(url, function(resp, stat) {
        if (stat == 'success') {
            var channel = resp.data;


        }
    });
}

function deleteChannel(id, gid) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var url = '/channels/delete?id=' + id + '&gid=' + gid;
        $.get(url, function(){
            layer.msg('删除成功!', {icon: 1, time: 1000});
            setTimeout(function() {
                window.location.reload();
            }, 1000);
        });
    });
}
