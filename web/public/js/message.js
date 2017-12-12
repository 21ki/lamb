
function formQuery(type) {
    var url = "";
    var method = "GET";

    switch (type) {
    case 1:
        url = "/message/getmessage";
        break;
    case 2:
        url = "/message/getdeliver";
        break;
    case 3:
        url = "/message/getreport";
        break;
    default:
        return;
    }

    url += "?";
    
    var where = $("form").serialize();
    url += where;
    var xhr = new XMLHttpRequest();
    xhr.responseType = "json";
    xhr.onreadystatechange = function() {
        if (xhr.readyState === xhr.DONE && xhr.status === 200) {
            var source = document.getElementById("contents").innerHTML;
            Handlebars.registerHelper('checkstatus', function(val) {
                switch (val) {
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
            });
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
        month += "0";
    }

    if (day >= 1 && day <= 9) {
        day += "0";
    }
    
    return now.getFullYear() + '-' + month + '-' + day + " " + time;
}
