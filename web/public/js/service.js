
function startup() {
    var method = "GET";
    var url = '/service/getcore';
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("contents").innerHTML;
            Handlebars.registerHelper('checkstatus', checkStatus)
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

function checkStatus(val) {
    var text;

    if (val) {
        text = '<span class="label label-success">正 常</span>';
    } else {
        text = '<span class="label label-default">未 知</span>';
    }

    return new Handlebars.SafeString(text);
}
