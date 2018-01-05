
function queryMessage() {
    var method = "GET";
    var url = "/message/getmessage";
    url += "?";
    var where = $("form").serialize();
    url += where;
    loading();
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            Handlebars.registerHelper('checkstatus', checkStatus);
            Handlebars.registerHelper('checkoverflow', checkOverflow);
            var source = document.getElementById("contents").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);

            $(".contents tbody").empty();
            $(".contents tbody").append(contents);
            $(".contents tbody tr").hide();
            $(".contents tbody tr").each(function(i){
                $(this).delay(i * 20).fadeIn(180);
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function queryDeliver() {
    var method = "GET";
    var url = "/message/getdeliver";
    url += "?";
    var where = $("form").serialize();
    url += where;
    loading();
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            Handlebars.registerHelper('checkoverflow', checkOverflow);
            var source = document.getElementById("contents").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);

            $(".contents tbody").empty();
            $(".contents tbody").append(contents);
            $(".contents tbody tr").hide();
            $(".contents tbody tr").each(function(i){
                $(this).delay(i * 20).fadeIn(180);
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function queryReport() {
    var method = "GET";
    var url = "/message/getreport";
    url += "?";
    var where = $("form").serialize();
    url += where;
    loading();
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            Handlebars.registerHelper('checkstatus', checkStatus);
            var source = document.getElementById("contents").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);

            $(".contents tbody").empty();
            $(".contents tbody").append(contents);
            $(".contents tbody tr").hide();
            $(".contents tbody tr").each(function(i){
                $(this).delay(i * 20).fadeIn(180);
            });
        }
    }
    xhr.open(method, url, true);
    xhr.send();
}

function queryStatistic() {
    var method = "GET";
    var url = "/message/getstatistic";
    url += "?";
    var where = $("form").serialize();
    url += where;
    loading();
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            layer.closeAll();
            Handlebars.registerHelper('gettype', getType);
            Handlebars.registerHelper('checkstatus', checkStatus);
            Handlebars.registerHelper('getobject', getObject);
            Handlebars.registerHelper('getdatetime', getdatetime);
            var source = document.getElementById("contents").innerHTML;
            var template = Handlebars.compile(source);
            var contents = template(xhr.response);
            $(".contents tbody").empty();
            $(".contents tbody").append(contents);
            $(".contents tbody tr").hide();
            $(".contents tbody tr").each(function(i){
                $(this).delay(i * 20).fadeIn(180);
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
        month = "0" + month;
    }

    if (day >= 1 && day <= 9) {
        day = "0" + day;
    }
    
    return now.getFullYear() + '-' + month + '-' + day + " " + time;
}

function loading() {
    layer.load(2, {
        shade: [0.1,'#cccccc']
    });
}

function checkOverflow(val, len) {
    if (typeof val === "string") {
        if (val.length > len) {
            return val.substring(0, len) + '...';
        }
    }

    return val;
}

function checkStatus(val) {
    switch (val) {
    case -1:
        return '全部状态';
    case 0:
        return 'WAITING';
    case 1:
        return 'DELIVRD';
    case 2:
        return 'EXPIRED';
    case 3:
        return 'DELETED';
    case 4:
        return 'UNDELIV';
    case 5:
        return 'ACCEPTD';
    case 6:
        return 'UNKNOWN';
    case 7:
        return 'REJECTD';
    default:
        return 'UNKNOWN';
    }
}

function getObject() {
    return $("input[name=object]").val();
}

function getdatetime(type) {
    switch (type) {
    case 'begin':
        return $("input[name=begin]").val();
    case 'end':
        return $("input[name=end]").val();
    default:
        return '';
    }
}

function getType() {
    return $("select[name=type]").find("option:selected").text();
}
