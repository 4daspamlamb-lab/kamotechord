# SPDX-License-Identifier: GPL-3.0-only
# KamotKeys Remote Script for Ableton Live (10.1+, 11, 12).
from __future__ import absolute_import, print_function, unicode_literals
from .KamotKeys import KamotKeys


def create_instance(c_instance):
    return KamotKeys(c_instance)
