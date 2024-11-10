from flask import Flask, jsonify, request, send_file
import app_config as conf
import playback_state
import json

app = Flask(__name__, static_url_path='', static_folder='web/static', template_folder='web/templates')
g_state = playback_state.PlaybackState()
g_config = conf.config()

@app.route(conf.ACTION_ROUTE, methods=["POST"])
def action():
    actions = request.json.get("actions")
    if not actions:
        return jsonify({"status":"No actions provided"})

    
    for action in actions:
        try:
            a = conf.apiAction.from_dict(action)
            g_state.perform_action(a)
            actionStatus ={"action": a[conf.ACTION], "status": "success"}
        except ValueError as e:
            actionStatus = {"action": a[conf.ACTION], "status": str(e)}

    # return the actions that were performed
    return jsonify(actionStatus)

@app.route(conf.CONFIG_ROUTE, methods=["POST"])
def update_config():
    try:
        if request.json == {}:
            return jsonify({"status": "no config data"})
        
        g_state.update_config_file(request.json)
        
        return jsonify({"status":"success"})    
    except Exception as e:
        return jsonify({"status": str(e)})
        

@app.route(conf.CONFIG_ROUTE, methods=["GET"])
def get_config():
    g_state.check_state()
    try:
        return jsonify({"status":"success","config":(g_state.read_config_file()),"state": g_state.get_state(),"volume":0,"activesong":g_state.active_file()})
    except Exception as e:
        return jsonify({"status": str(e)})


@app.route(conf.FILE_ROUTE, methods=["GET"])
def get_file_list():
    # print(g_state.get_files())
    try:
        return jsonify({"files": g_state.get_files(), "status": "success"})
    except Exception as e:
        return jsonify({"status": str(e)})


@app.route("/api/background",methods=["GET"])
def get_image():
    return send_file("image.png")