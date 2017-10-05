{% extends "base.tpl" %}
{% block body %}

<style type="text/css" scoped>
  .panel_header {
    background-color: #03223e;
    overflow: auto;
    margin-top: 0;
    margin-bottom: 0;
    padding-top: 10px;
    padding-bottom: 10px;
    padding-left: 20px;
    font-size:18px;
  }

  .body_row {
    margin: 0;
    padding-right: 20px;
  }

  .body_col {
    padding-left: 20px;
    padding-right: 0;
  }

  .debug_item {
    overflow: auto;
    border: 0;
    background: rgba(3, 41, 75, 0.4);
    margin-bottom: 0;
    font-size: 16px;
    padding: 0;
  }

  .debug_item + .light {
    background: rgba(3, 41, 75, 1);
  }

  .debug_item .item_content {
    margin: 12px 20px;
  }

  .hmi_small_btn, .hmi_small_btn:hover, .hmi_small_btn:focus {
    width: 60px;
    height: 30px;
    background: rgba(15, 62, 98, 0.34);
    border: 1px solid #30a5ff;
    border-radius: 0;
    color: #30a5ff;
  }

  .hmi_small_btn:active {
    opacity: 0.6;
  }

  .hmi_small_btn:disabled {
    opacity: 0.2;
    background: rgba(255, 255, 255, 0.20);
    border: 1px solid #ffffff;
    color: #ffffff;
  }

  .glyphicon-ok {
    color: green;
  }

  .glyphicon-info-sign {
    color: yellow;
  }

  .glyphicon-remove {
    color: red;
  }

  @keyframes rotate {
    from {transform: rotate(0deg);}
    to {transform: rotate(360deg);}
  }

  .rotating {
    animation: 1s linear 0s normal none infinite rotate;
  }
</style>

<div class="row body_row">
  <div class="col-md-4 body_col">
    {% include 'cards/left_panel.tpl' %}
  </div>
  <div class="col-md-4 body_col">
    {% include 'cards/center_panel.tpl' %}
  </div>
  <div class="col-md-4 body_col">
    {% include 'cards/right_panel.tpl' %}
  </div>
</div>

<script>
  function goto_dreamview() {
    javascript:window.location.port = 8888;
  }

  // Change UI according to status.
  function on_status_change(global_status) {
    on_hardware_status_change(global_status);
    on_modules_status_change(global_status);
    on_tools_status_change(global_status);
    on_config_status_change(global_status);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Client-server communication via socketio.
  //////////////////////////////////////////////////////////////////////////////
  socketio = undefined;
  // Parameters are mapped to apollo::hmi::SocketIORequest.
  function io_request(api, cmd_name, cmd_args=[]) {
    socketio.emit('socketio_api',
                  {api_name: api, command_name: cmd_name, args: cmd_args});
  }

  $(document).ready(function() {
    init_modules_panel();

    // Get current status once to init UI elements.
    $.getJSON('{{ url_for('runtime_status') }}', function( status_json ) {
      on_status_change(status_json);
    });

    // Setup socketio to communicate with HMI server.
    socketio = io.connect(
        '{{ protocol }}://' + document.domain + ':' + location.port + '/io_frontend');
    socketio.on('current_status', function(status_json) {
      on_status_change(status_json);
    });
  });
</script>

<script src="https://cdnjs.cloudflare.com/ajax/libs/socket.io/1.7.4/socket.io.min.js"></script>
{% endblock %}
