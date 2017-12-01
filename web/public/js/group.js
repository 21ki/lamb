function show() {
    var template = document.getElementById("new-page").innerHTML;

    layer.open({
        type: 1,
        title: '创建通道组',
        area: ['435px', '285px'],
        content: template
    });
}

function editGroup(id) {
    var template = document.getElementById("edit-page").innerHTML;

    layer.open({
        type: 1,
        title: '编辑通道组',
        area: ['435px', '285px'],
        content: template
    });  

    var url = '/api/group?id=' + id;
    $.get(url, function(resp, stat){
        if (stat == 'success') {
            var g = resp.data;
            $("input[name=id]").val(g.id);
            $("input[name=name]").val(g.name);
            $("input[name=description]").val(g.description);
        }
    });
}

function deleteGroup(gid) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var url = '/groups/delete?id=' + gid;
        $.get(url, function(){
            layer.msg('删除成功!', {icon: 1, time: 1000});
            setTimeout(function() {
                window.location.reload();
            }, 1000);
        });
    });
}

