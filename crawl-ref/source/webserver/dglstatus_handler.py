import logging
import subprocess

import tornado.web
import tornado.gen

import ws_handler
from conf import config


class DglStatusHandler(tornado.web.RequestHandler):

    """Handle dgl-status requests."""

    def get(self, arg=None):
        """Return dgl-status.

        We compose this by splicing together dgl_status_file and dgamelaunch -s
        dgamelaunch doesn't neccessarily know complete info about webtiles
        games. So get its info first, then take our own internal game list and
        use that as authoritative information if there are duplicates.
        """
        expose_dgl_status = config.get('expose_dgl_status')
        dgamelaunch = config.get('dgamelaunch_path')
        if not expose_dgl_status:
            self.fail(500, "dgl-status is not available.")
            return

        game_info = {}

        if dgamelaunch:
            try:
                p = subprocess.Popen(
                    [dgamelaunch, '-s'],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE)
            except OSError as e:
                self.fail(500, "dgl-status could not execute: "
                          "{}".format(e.strerror))
                return
            stdout, stderr = p.communicate()
            if stdout:
                for game in stdout.splitlines():
                    nick = game.split('#', 1)[0]
                    game_info[nick] = game

        for game in ws_handler.all_games_info():
            nick = game[0]
            game_info[nick] = '#'.join(game)

        self.set_header("Content-Type", "text/plain")
        self.write('\n'.join(game_info.values()))

    def fail(self, code, message):
        """Handler override for error page pre-render tasks."""
        logging.debug("{0}: {1}: {2}".format(self.request.remote_ip, code,
                                             message))
        self.send_error(code, message=message)

    def write_error(self, status_code, **kwargs):
        """Handler override for error page rendering."""
        if "message" in kwargs:
            msg = "{0}: {1}".format(status_code, kwargs["message"])
            self.write("<html><body>{0}</body></html>".format(msg))
