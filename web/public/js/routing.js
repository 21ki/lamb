
function show() {
    var template = document.getElementById("create-page").innerHTML;
    layer.open({
        type: 1,
        title: '创建路由表',
        area: ['435px', '285px'],
        content: template
    });

    var url = '/api/groups';
    $.get(url, function(resp, stat){
        if (stat == 'success') {
            var el = resp.data;
            for (i in el) {
                var option = '<option value="' + el[i].id + '">' + el[i].name + '</option>'
                $(option).appendTo("#groups");
            }
        }
    });
}

function changeRouting(id) {
    var template = document.getElementById("edit-page").innerHTML;
    
    layer.open({
        type: 1,
        title: '编辑路由表',
        area: ['435px', '285px'],
        content: template
    });

    var url = '/api/routing?id=' + id;
    $.get(url, function(resp, stat){
        var routing = null;
        if (stat == 'success') {
            routing = resp.data;
            $("input[name=id]").val(routing.id);
            $("input[name=rexp]").val(routing.rexp);
            $("input[name=description]").val(routing.description);
        }

        if (routing == null) {
            $("<option>没有可用数据</option>").appendTo("#groups");
            return;
        }

        url = '/api/groups';
        $.get(url, function(resp, stat){
            if (stat == 'success') {
                var el = resp.data;
                for (i in el) {
                    var seld = (el[i].id == routing.target) ? ' selected="selected"' : '';
                    var option = '<option value="' + el[i].id + '"' + seld + '>' + el[i].name + '</option>';
                    $(option).appendTo("#groups");
                }
            }
        });

    });
}

function deleteRouting(id) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var url = '/routing/delete?id=' + id;
        $.get(url, function(){
            layer.msg('删除成功!', {icon: 1, time: 1000});
            setTimeout(function() {
                window.location.reload();
            }, 1000);
        });
    });
}

