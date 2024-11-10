from flask import Flask, jsonify, request
import app_config as conf
import playback_state

app = Flask(__name__, static_url_path='', static_folder='web/static', template_folder='web/templates')
g_state = playback_state.PlaybackState()
g_config = conf.config()

@app.route(conf.ACTION_ROUTE, methods=["POST"])
def action():
    print(request.json)
    actions = request.json.get("actions")
    if not actions:
        return jsonify({"No actions provided"})

    actionStatus = []
    for action in actions:
        try:
            a = conf.apiAction.from_dict(action)
            g_state.perform_action(a)
            actionStatus.append({"action": a[conf.ACTION], "status": "Success"})
        except ValueError as e:
            actionStatus.append({"action": a[conf.ACTION], "status": str(e)})

    # return the actions that were performed
    return jsonify(actionStatus)

@app.route(conf.CONFIG_ROUTE, methods=["POST"])
def update_config():
    config_data = conf.configVariable(request.json);  
    g_state.update_config_file(config_data)

@app.route(conf.CONFIG_ROUTE, methods=["GET"])
def get_config():
    return jsonify(g_config)
