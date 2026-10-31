import unittest
from deck_wire_receiver_reference import (
    Entry, Status, capture, encode, decode, fingerprint_guid_pool,
    validate_receiver,
)

class DeckWireReceiverTest(unittest.TestCase):
    def setUp(self):
        self.entries = [Entry(1, 0, 0, 709, -1), Entry(1, 0, 1, 709, 55),
                        Entry(1, 0, 2, 334, 56), Entry(2, 0, 0, 715, 70)]

    def test_roundtrip_and_receiver_virtual(self):
        request = capture(self.entries, self.entries[0])
        self.assertEqual(decode(encode(request)), request)
        self.assertEqual(validate_receiver(self.entries, request, 1).status,
                         Status.REQUIRES_MATERIALIZATION)

    def test_entity_resolution(self):
        req=capture(self.entries, self.entries[1])
        self.assertEqual(validate_receiver(self.entries, req, 1).entity_id, 55)

    def test_wrong_owner(self):
        req=capture(self.entries, self.entries[1])
        self.assertEqual(validate_receiver(self.entries, req, 2).status, Status.WRONG_OWNER)

    def test_pool_changed(self):
        req=capture(self.entries, self.entries[1])
        shuffle=[self.entries[1],self.entries[0],*self.entries[2:]]
        self.assertEqual(validate_receiver(shuffle, req, 1).status, Status.POOL_CHANGED)

    def test_clone_id_difference(self):
        req=capture(self.entries, self.entries[1])
        cloned=[Entry(x.owner,x.pool,x.index,x.guid,155 if x.entity_id==55 else x.entity_id)
                for x in self.entries]
        self.assertEqual(validate_receiver(cloned, req, 1).entity_id, 155)

    def test_invalid_and_ambiguous(self):
        req=capture(self.entries, self.entries[1]); encoded=list(encode(req))
        encoded[8]^=1
        self.assertIsNone(decode(encoded))
        duplicated=[*self.entries,Entry(1,0,3,334,55)]
        req2=capture(duplicated, duplicated[1])
        self.assertEqual(validate_receiver(duplicated,req2,1).status,Status.AMBIGUOUS_ENTITY)

    def test_fingerprint_is_stable_on_virtual_vs_materialized(self):
        original=fingerprint_guid_pool(self.entries,1,0)
        other=[Entry(x.owner,x.pool,x.index,x.guid,99 if x.entity_id==-1 else x.entity_id)
               for x in self.entries]
        self.assertEqual(original,fingerprint_guid_pool(other,1,0))

if __name__=='__main__': unittest.main()
