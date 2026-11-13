# SPDX-License-Identifier: GPL-3.0-only
# KamoteKeys Remote Script for Ableton Live (10.1+, 11, 12).
from __future__ import absolute_import, print_function, unicode_literals
from .KamoteKeys import KamoteKeys


def create_instance(c_instance):
    return KamoteKeys(c_instance)
