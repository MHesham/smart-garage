#!/usr/bin/env python

from collections import deque
import json
from event import Event, EventManager
import logging

log = logging.getLogger(__name__)


class LedStrip(object):
    TOPIC_CONFIG = "light/config"
    TOPIC_STATE = "light/state"

    def __init__(self, evt_mgr: EventManager):
        self.evt_mgr = evt_mgr
        self.state = {'enabled': False, 'color': 'black'}
        self.power_off()

    def power_on(self, color: str = 'white'):
        log.debug('power_on')
        self.state['enabled'] = True
        self.state['color'] = color
        self.evt_mgr.publish(
            Event(self.TOPIC_CONFIG, self.state))

    def power_off(self):
        log.debug('power_off')
        self.state['enabled'] = False
        self.evt_mgr.publish(
            Event(self.TOPIC_CONFIG, self.state))


class MotionSensor(object):
    TOPIC_STATE = "motion/state"
    TOPIC_MOTION_ACTIVE_CHANGED = "motion/state/active/changed"

    @property
    def is_active(self):
        return self.state['active']

    def __init__(self, evt_mgr: EventManager):
        self.evt_mgr = evt_mgr
        self.evt_mgr.subscribe(self.TOPIC_STATE, self.__on_state)
        self.state = {'active': False}

    def __on_state(self, evt: Event):
        log.debug('Current: ' + str(self.state))
        log.debug('New: ' + str(evt.value))
        if evt.value['active'] != self.state['active']:
            self.evt_mgr.publish(
                Event(self.TOPIC_MOTION_ACTIVE_CHANGED))
        self.state = evt.value
