# -*- coding: utf-8 -*-

import b3
import b3.plugin


__author__ = 'XV9K'
__version__ = '1.0'


class CodxcommandsPlugin(b3.plugin.Plugin):

    _adminPlugin = None


    def startup(self):

        self.debug('Loading Codxcommands plugin')

        self._adminPlugin = self.console.getPlugin('admin')

        if not self._adminPlugin:
            self.error('Admin plugin missing')
            return False

        self.registerCommands()

        return True



    def registerCommands(self):

        if 'commands' not in self.config.sections():
            return

        for cmd in self.config.options('commands'):

            level = self.config.get('commands', cmd)

            alias = None

            if '-' in cmd:
                cmd, alias = cmd.split('-', 1)

            func = self.getCmd(cmd)

            if func:
                self._adminPlugin.registerCommand(
                    self,
                    cmd,
                    level,
                    func,
                    alias
                )

                self.debug('Registered command !%s' % cmd)



    def getCmd(self, cmd):

        func = 'cmd_%s' % cmd

        if hasattr(self, func):
            return getattr(self, func)

        return None

    #
    # !bb - Nektum Shield Ban
    #
    def cmd_bb(self, data, client, cmd=None):
        if not data:
            client.message('^7Usage: ^2!bb <query> <reason>')
            client.message('^7Query can be: ^2Name, PID, NID, Subnet or full IP')
            return
        
        self.console.write('bb %s %s' % (client.cid, data))

    #
    # !ub - Nektum Shield Unban
    #
    def cmd_ub(self, data, client, cmd=None):
        if not data:
            client.message('^7Usage: ^2!ub <query>')
            client.message('^7Query can be: ^2Name, PID, NID, Subnet or full IP')
            return
        
        self.console.write('ub %s %s' % (client.cid, data))

    #
    # !fu - Nektum Shield Find User
    #
    def cmd_fu(self, data, client, cmd=None):
        if not data:
            client.message('^7Usage: ^2!fu <query>')
            client.message('^7Query can be: ^2Name, PID, NID, Subnet or full IP')
            return
        
        self.console.write('fu %s %s' % (client.cid, data))
        client.message('^7Command sent to Nektum Shield: ^2!fu %s' % data)

    #
    # !mute - Nektum Shield Mute (Persistent)
    #
    def cmd_mute(self, data, client, cmd=None):
        if not data:
            client.message('^7Usage: ^2!mute ^7<player> <reason>')
            return

        args = data.split(' ', 1)
        if len(args) < 2:
            client.message('^7Usage: ^2!mute ^7<player> <reason>')
            return

        target_name = args[0]
        reason = args[1]

        target = self._adminPlugin.findClientPrompt(target_name, client)
        if not target:
            return

        self.console.write('mute %s %s %s' % (client.cid, target_name, reason))

    #
    # !unmute - Nektum Shield Unmute (Persistent)
    #
    def cmd_unmute(self, data, client, cmd=None):
        if not data:
            client.message('^7Usage: ^2!unmute ^7<player>')
            return

        target = self._adminPlugin.findClientPrompt(data, client)
        if not target:
            return

        self.console.write('unmute %s %s' % (client.cid, target.name))