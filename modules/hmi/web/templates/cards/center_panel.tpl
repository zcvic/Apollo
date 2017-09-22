<div>
  <style type="text/css" scoped>
    .module_switch {
      width: 40px;
      height: 20px;
      border-radius: 62px;
    }

    .module_switch .module_switch_btn {
      width: 16px;
      height: 16px;
      margin: 2px;
      border-radius: 100%;
    }

    .module_switch_open {
      background: #0e3d62;
    }

    .module_switch_open .module_switch_btn {
      float: right;
      background: #30a5ff;
    }

    .module_switch_close {
      background: #2a2e30;
    }

    .module_switch_close .module_switch_btn {
      float: left;
      background: #6e6e6e;
    }
</style>

  <h3>Controllers</h3>
  <h2 class="panel_header">Modules</h2>

  <ul class="list-group">
    {% for module in conf_pb.modules %}
    <li class="list-group-item debug_item
        {% if loop.index % 2 == 0 %} light {% endif %}">
      <div class="item_content" id="module_{{ module.name }}">
        {{ module.display_name }}
        <div class="pull-right module_switch module_switch_close"
            ><div class="module_switch_btn"></div></div>
      </div>
    </li>
    {% endfor %}
  </ul>

</div>

<script>
  function init_modules_panel() {
    var modules = [{% for module in conf_pb.modules %} '{{ module.name }}', {% endfor %}];
    modules.forEach(function(module_name) {
      // Init module switches.
      $btn = $("#module_" + module_name + " .module_switch");
      $btn.click(function(e) {
        if ($(this).hasClass("module_switch_close")) {
          io_request('module_api', 'start', [module_name]);
        } else {
          io_request('module_api', 'stop', [module_name]);
        }
        $(this).toggleClass("module_switch_close module_switch_open");
      });
    });
  }

  // Change UI according to status.
  function on_modules_status_change(global_status) {
    global_status['modules'].forEach(function(module_status) {
      // Get module status.
      status_code = 'status' in module_status ? module_status['status']
                                              : 'UNINITIALIZED';
      $btn = $("#module_" + module_status['name'] + " .module_switch");
      if (status_code == 'STARTED' || status_code == 'INITIALIZED') {
        $btn.removeClass("module_switch_close").addClass("module_switch_open");
      } else {
        $btn.removeClass("module_switch_open").addClass("module_switch_close");
      }
    });
  }
</script>
