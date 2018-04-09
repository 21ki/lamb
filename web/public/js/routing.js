function  startup() {
    var method = "GET";
    var url = "/routing/summary";
    var xhr = new XMLHttpRequest();

    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
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

function cleanup(id) {
    layer.confirm('亲，确定要删除？', {
        btn: ['是','否'], icon: 3
    }, function(){
        var method = "DELETE";
        var url = "/routing/cleanup?id=" + id;
        var xhr = new XMLHttpRequest();

        xhr.responseType = "json";
        xhr.onreadystatechange = function() {
            if (xhr.readyState === xhr.DONE && xhr.status === 200) {
                if (xhr.response.status === 200) {
                    layer.msg('清空路由成功!', {icon: 1, time: 1000});
                } else {
                    layer.msg('清空路由失败!', {icon: 2, time: 1000});
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
