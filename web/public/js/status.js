
function inbound() {
    var method = "GET";
    var url = '/api/inbound';
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("contents").innerHTML;
            Handlebars.registerHelper('checkstatus', checkStatus);
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

function outbound() {
    var method = "GET";
    var url = '/api/outbound';
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function(){
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("contents").innerHTML;
            Handlebars.registerHelper('checkstatus', checkStatus);
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
    val = parseInt(val);
    var text = '<span class="label label-danger">超 时</span>';

    if (val == 1) {
        text = '<span class="label label-success">OK</span>';
    } else if (val == -1) {
        text = '<span class="label label-default">未 知</span>';
    }

    return new Handlebars.SafeString(text);
}
